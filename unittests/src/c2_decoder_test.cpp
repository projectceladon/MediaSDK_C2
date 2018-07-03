/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_defs.h"
#include <gtest/gtest.h>
#include "test_components.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "C2PlatformSupport.h"
#include "streams/h264/aud_mw_e.264.h"
#include "streams/h264/freh9.264.h"
#include "streams/h265/AMVP_A_MTK_4.bit.h"
#include "streams/h265/CAINIT_A_SHARP_4.bit.h"

#include <future>
#include <set>

using namespace android;

const uint64_t FRAME_DURATION_US = 33333; // 30 fps
const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

static std::vector<C2ParamDescriptor> dec_params_desc =
{
    { false, "MemoryType", C2MemoryTypeSetting::PARAM_TYPE },
};

namespace {
    struct ComponentDesc
    {
        const char* component_name;
        int flags;
        c2_status_t creation_status;
        std::vector<C2ParamDescriptor> params_desc;
        std::vector<std::vector<const StreamDescription*>> streams;
    };

    // This function is used by ::testing::PrintToStringParamName to give
    // parameterized tests reasonable names instead of /1, /2, ...
    void PrintTo(const ComponentDesc& desc, ::std::ostream* os)
    {
        PrintAlphaNumeric(desc.component_name, os);
    }

    class CreateDecoder : public ::testing::TestWithParam<ComponentDesc>
    {
    };
    // Test fixture class called Decoder to beautify output
    class Decoder : public ::testing::TestWithParam<ComponentDesc>
    {
    };
}

static std::vector<std::vector<const StreamDescription*>> h264_streams =
{
    { &aud_mw_e_264 },
    { &freh9_264 },
    { &aud_mw_e_264, &freh9_264 },
    { &freh9_264, &aud_mw_e_264 }
};

static std::vector<std::vector<const StreamDescription*>> h265_streams =
{
    { &AMVP_A_MTK_4_bit },
    { &CAINIT_A_SHARP_4_bit },
    { &AMVP_A_MTK_4_bit, &CAINIT_A_SHARP_4_bit },
    { &CAINIT_A_SHARP_4_bit, &AMVP_A_MTK_4_bit }
};

static ComponentDesc g_components_desc[] = {
    { "C2.h264vd", 0, C2_OK, dec_params_desc, h264_streams },
    { "C2.h265vd", 0, C2_OK, dec_params_desc, h265_streams },
};

static ComponentDesc g_invalid_components_desc[] = {
    { "C2.NonExistingDecoder", 0, C2_NOT_FOUND, {}, {} },
};

// Assures that all decoding components might be successfully created.
// NonExistingDecoder cannot be created and C2_NOT_FOUND error is returned.
TEST_P(CreateDecoder, Create)
{
    const ComponentDesc& desc = GetParam();

    std::shared_ptr<MfxC2Component> decoder = GetCachedComponent(desc);

    EXPECT_EQ(decoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;
}

// Checks that all successfully created decoding components expose C2ComponentInterface
// and return correct information once queried (component name).
TEST_P(Decoder, intf)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        EXPECT_EQ(comp_intf->getName(), desc.component_name);
    } );
}

// Checks list of actually supported parameters by all decoding components.
// Parameters order doesn't matter.
// For every parameter index, name, required and persistent fields are checked.
TEST_P(Decoder, getSupportedParams)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        std::vector<std::shared_ptr<C2ParamDescriptor>> params_actual;
        c2_status_t sts = comp_intf->querySupportedParams_nb(&params_actual);
        EXPECT_EQ(sts, C2_OK);

        EXPECT_EQ(desc.params_desc.size(), params_actual.size());

        for(const C2ParamDescriptor& param_expected : desc.params_desc) {

            const auto found_actual = std::find_if(params_actual.begin(), params_actual.end(),
                [&] (auto p) { return p->index() == param_expected.index(); } );

            EXPECT_NE(found_actual, params_actual.end())
                << "missing parameter " << param_expected.name();
            if (found_actual != params_actual.end()) {
                EXPECT_EQ((*found_actual)->isRequired(), param_expected.isRequired());
                EXPECT_EQ((*found_actual)->isPersistent(), param_expected.isPersistent());
                EXPECT_EQ((*found_actual)->name(), param_expected.name());
            }
        }
    } );
}

static void PrepareWork(uint32_t frame_index,
    std::shared_ptr<const C2Component> component,
    std::unique_ptr<C2Work>* work,
    const std::vector<char>& bitstream, bool end_stream, bool header)
{
    *work = std::make_unique<C2Work>();
    C2FrameData* buffer_pack = &((*work)->input);

    buffer_pack->flags = C2FrameData::flags_t(0);
    if (header)
        buffer_pack->flags = C2FrameData::flags_t(buffer_pack->flags | C2FrameData::FLAG_CODEC_CONFIG);
    if (end_stream)
        buffer_pack->flags = C2FrameData::flags_t(buffer_pack->flags | C2FrameData::FLAG_END_OF_STREAM);

    // Set up frame header properties:
    // timestamp is set to correspond to 30 fps stream.
    buffer_pack->ordinal.timestamp = FRAME_DURATION_US * frame_index;
    buffer_pack->ordinal.frameIndex = frame_index;
    buffer_pack->ordinal.customOrdinal = 0;

    do {

        std::shared_ptr<C2BlockPool> allocator;
        c2_status_t sts = GetCodec2BlockPool(C2BlockPool::BASIC_LINEAR,
            component, &allocator);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(allocator, nullptr);

        if(nullptr == allocator) break;

        C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        std::shared_ptr<C2LinearBlock> block;
        sts = allocator->fetchLinearBlock(bitstream.size(),
            mem_usage, &block);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(block, nullptr);

        if(nullptr == block) break;

        std::unique_ptr<C2WriteView> write_view;
        sts = MapLinearBlock(*block, TIMEOUT_NS, &write_view);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(write_view, nullptr);

        uint8_t* data = write_view->data();
        EXPECT_NE(data, nullptr);

        memcpy(data, &bitstream.front(), bitstream.size());

        C2Event event;
        event.fire(); // pre-fire as buffer is already ready to use
        C2ConstLinearBlock const_block = block->share(0, bitstream.size(), event.fence());
        // make buffer of linear block
        std::shared_ptr<C2Buffer> buffer = std::make_shared<C2Buffer>(MakeC2Buffer( { const_block } ));

        buffer_pack->buffers.push_back(buffer);

        std::unique_ptr<C2Worklet> worklet = std::make_unique<C2Worklet>();
        // C2 requires output items be allocated in buffers list and set to nulls
        worklet->output.buffers.push_back(nullptr);
        // work of 1 worklet
        (*work)->worklets.push_back(std::move(worklet));
    } while(false);
}

class DecoderConsumer : public C2Component::Listener
{
public:
    typedef std::function<void(uint32_t width, uint32_t height,
        const uint8_t* data, size_t length)> OnFrame;

public:
    DecoderConsumer(OnFrame on_frame)
        :on_frame_(on_frame)
        ,frame_submitted_(0)
        ,frame_decoded_(0)
    {
    }

    // future ready when validator got all expected frames
    std::future<void> GetFuture()
    {
        return done_.get_future();
    }

    std::string GetMissingFramesList()
    {
        std::ostringstream ss;
        bool first = true;
        for (const auto& index : frame_submitted_set_) {
            if (!first) ss << ";";
            ss << index;
            first = false;
        }
        return ss.str();
    }

    void SetSubmittedFrames(uint64_t frame_submitted)
    {
        frame_submitted_ = frame_submitted;

        for (uint64_t i = 0; i < frame_submitted; ++i) {
            frame_submitted_set_.insert(i);
        }
    }

protected:
    void onWorkDone_nb(
        std::weak_ptr<C2Component> component,
        std::list<std::unique_ptr<C2Work>> workItems) override
    {
        (void)component;

        for(std::unique_ptr<C2Work>& work : workItems) {
            EXPECT_EQ(work->workletsProcessed, 1u);
            EXPECT_EQ(work->result, C2_OK);
            EXPECT_EQ(work->worklets.size(), 1u);
            if (work->worklets.size() >= 1) {

                std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
                C2FrameData& buffer_pack = worklet->output;

                uint64_t frame_index = buffer_pack.ordinal.frameIndex.peeku();

                frame_submitted_set_.erase(frame_index);

                EXPECT_EQ(buffer_pack.ordinal.timestamp, frame_index * FRAME_DURATION_US); // 30 fps

                EXPECT_EQ(!frame_submitted_ || (frame_index < frame_submitted_), true)
                    << "unexpected frame_index value" << frame_index;

                ++frame_decoded_;

                std::unique_ptr<C2ConstGraphicBlock> graphic_block;
                c2_status_t sts = GetC2ConstGraphicBlock(buffer_pack, &graphic_block);
                EXPECT_EQ(sts, C2_OK);

                if(nullptr != graphic_block) {

                    C2Rect crop = graphic_block->crop();
                    EXPECT_NE(crop.width, 0u);
                    EXPECT_NE(crop.height, 0u);

                    std::unique_ptr<const C2GraphicView> c_graph_view;
                    sts = MapConstGraphicBlock(*graphic_block, TIMEOUT_NS, &c_graph_view);
                    EXPECT_EQ(sts, C2_OK) << NAMED(sts);
                    EXPECT_TRUE(c_graph_view);

                    if (c_graph_view) {
                        C2PlanarLayout layout = c_graph_view->layout();

                        const uint8_t* const* raw  = c_graph_view->data();

                        EXPECT_NE(raw, nullptr);
                        for (uint32_t i = 0; i < layout.numPlanes; ++i) {
                            EXPECT_NE(raw[i], nullptr);
                        }

                        std::shared_ptr<std::vector<uint8_t>> data_buffer = std::make_shared<std::vector<uint8_t>>();
                        data_buffer->resize(crop.width * crop.height * 3 / 2);
                        uint8_t* raw_cropped = &(data_buffer->front());
                        uint8_t* raw_cropped_chroma = raw_cropped + crop.width * crop.height;
                        const uint8_t* raw_chroma = raw[C2PlanarLayout::PLANE_U];

                        for (uint32_t i = 0; i < crop.height; i++) {
                            const uint32_t stride = layout.planes[C2PlanarLayout::PLANE_Y].rowInc;
                            memcpy(raw_cropped + i * crop.width, raw[C2PlanarLayout::PLANE_Y] + (i + crop.top) * stride + crop.left, crop.width);
                        }
                        for (uint32_t i = 0; i < (crop.height >> 1); i++) {
                            const uint32_t stride = layout.planes[C2PlanarLayout::PLANE_U].rowInc;
                            memcpy(raw_cropped_chroma + i * crop.width, raw_chroma + (i + (crop.top >> 1)) * stride + crop.left, crop.width);
                        }

                        if(nullptr != raw_cropped) {
                            on_frame_(crop.width, crop.height,
                                raw_cropped, crop.width * crop.height * 3 / 2);
                        }
                    }
                }
            }
        }
        // if collected all expected frames
        if(frame_submitted_ && (frame_decoded_ == frame_submitted_)) {
            done_.set_value();
        }
    }

    void onTripped_nb(std::weak_ptr<C2Component> component,
                           std::vector<std::shared_ptr<C2SettingResult>> settingResult) override
    {
        (void)component;
        (void)settingResult;
        EXPECT_EQ(true, false) << "onTripped_nb callback shouldn't come";
    }

    void onError_nb(std::weak_ptr<C2Component> component,
                         uint32_t errorCode) override
    {
        (void)component;
        (void)errorCode;
        EXPECT_EQ(true, false) << "onError_nb callback shouldn't come";
    }

private:
    OnFrame on_frame_;
    uint64_t frame_submitted_; // number of the frames submitted for decoding
    uint64_t frame_decoded_;   // number of decoded frames
    std::set<uint64_t> frame_submitted_set_;
    std::promise<void> done_;  // fire when all expected frames came
};

static void Decode(
    bool graphics_memory,
    std::shared_ptr<C2Component> component,
    std::shared_ptr<DecoderConsumer> validator,
    const std::vector<const StreamDescription*>& streams)
{
    c2_blocking_t may_block {};
    component->setListener_vb(validator, may_block);

    C2MemoryTypeSetting setting;
    setting.value = graphics_memory ? C2MemoryTypeGraphics : C2MemoryTypeSystem;

    std::vector<C2Param*> params = { &setting };
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    std::shared_ptr<C2ComponentInterface> comp_intf = component->intf();

    c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
    EXPECT_EQ(sts, C2_OK);

    sts = component->start();
    EXPECT_EQ(sts, C2_OK);

    std::unique_ptr<StreamReader> reader { StreamReader::Create(streams) };
    uint32_t frame_index = 0;

    StreamDescription::Region region {};
    bool header = false;

    bool res = reader->Read(StreamReader::Slicing::Frame(), &region, &header);
    while(res) {
        // prepare worklet and push
        std::unique_ptr<C2Work> work;

        // insert input data
        PrepareWork(frame_index, component, &work, reader->GetRegionContents(region),
            reader->EndOfStream(), header);
        std::list<std::unique_ptr<C2Work>> works;
        works.push_back(std::move(work));

        frame_index++;

        res = reader->Read(StreamReader::Slicing::Frame(), &region, &header);
        if (!res) {
            validator->SetSubmittedFrames(frame_index);
        }

        sts = component->queue_nb(&works);
        EXPECT_EQ(sts, C2_OK);
    }

    std::future<void> future = validator->GetFuture();
    std::future_status future_sts = future.wait_for(std::chrono::seconds(10));

    EXPECT_EQ(future_sts, std::future_status::ready) << " decoded less frames than expected, missing: "
        << validator->GetMissingFramesList();

    component->setListener_vb(nullptr, may_block);
    sts = component->stop();
    EXPECT_EQ(sts, C2_OK);
}

static std::string GetStreamsCombinedName(const std::vector<const StreamDescription*>& streams)
{
    std::ostringstream res;

    bool first = true;
    for (const auto& stream : streams) {
        if (!first) {
            res << "-";
        }
        res << stream->name;
        first = false;
    }
    return res.str();
}

TEST_P(Decoder, DecodeBitExact)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        const int TESTS_COUNT = 5;

        // odd runs are on graphics memory
        auto use_graphics_memory = [] (int i) -> bool { return (i % 2) != 0; };
        std::map<bool, std::string> memory_names = {
            { false, "(system memory)" },
            { true, "(video memory)" },
        };

        for(const std::vector<const StreamDescription*>& streams : desc.streams) {
            for(int i = 0; i < TESTS_COUNT; ++i) {

                CRC32Generator crc_generator;

                GTestBinaryWriter writer(std::ostringstream()
                    << comp_intf->getName() << "-" << GetStreamsCombinedName(streams) << "-" << i << ".nv12");

                DecoderConsumer::OnFrame on_frame = [&] (uint32_t width, uint32_t height,
                    const uint8_t* data, size_t length) {

                    writer.Write(data, length);
                    crc_generator.AddData(width, height, data, length);
                };

                std::shared_ptr<DecoderConsumer> validator =
                    std::make_shared<DecoderConsumer>(on_frame);

                Decode(use_graphics_memory(i), comp, validator, streams);

                std::list<uint32_t> actual_crc = crc_generator.GetCrc32();
                std::list<uint32_t> expected_crc;
                std::transform(streams.begin(), streams.end(), std::back_inserter(expected_crc),
                    [] (const StreamDescription* stream) { return stream->crc32_nv12; } );

                EXPECT_EQ(actual_crc, expected_crc) << "Pass " << i << " not equal to reference CRC32"
                    << memory_names[use_graphics_memory(i)];
            }
        }
    } );
}

// Checks the correctness of all decoding components state machine.
// The component should be able to start from STOPPED (initial) state,
// stop from RUNNING state. Otherwise, C2_BAD_STATE should be returned.
TEST_P(Decoder, State)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr) {

        c2_status_t sts = C2_OK;

        sts = comp->start();
        EXPECT_EQ(sts, C2_OK);

        sts = comp->start();
        EXPECT_EQ(sts, C2_BAD_STATE);

        sts = comp->stop();
        EXPECT_EQ(sts, C2_OK);

        sts = comp->stop();
        EXPECT_EQ(sts, C2_BAD_STATE);
    } );
}

INSTANTIATE_TEST_CASE_P(MfxComponents, CreateDecoder,
    ::testing::ValuesIn(g_components_desc),
    ::testing::PrintToStringParamName());

INSTANTIATE_TEST_CASE_P(MfxInvalidComponents, CreateDecoder,
    ::testing::ValuesIn(g_invalid_components_desc),
    ::testing::PrintToStringParamName());

INSTANTIATE_TEST_CASE_P(MfxComponents, Decoder,
    ::testing::ValuesIn(g_components_desc),
    ::testing::PrintToStringParamName());
