/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_defs.h"
#include "gtest_emulation.h"
#include "test_components.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "C2BlockAllocator.h"
#include "streams/h264/aud_mw_e.264.h"
#include "streams/h264/freh9.264.h"

#include <future>
#include <set>

using namespace android;

const uint64_t FRAME_DURATION_US = 33333; // 30 fps
const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

static std::vector<C2ParamDescriptor> h264_params_desc =
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
}

static std::vector<std::vector<const StreamDescription*>> h264_streams =
{
    { &aud_mw_e_264 },
    { &freh9_264 },
    { &aud_mw_e_264, &freh9_264 },
    { &freh9_264, &aud_mw_e_264 }
};

static ComponentDesc g_components_desc[] = {
    { "C2.h264vd", 0, C2_OK, h264_params_desc, h264_streams },
    { "C2.NonExistingDecoder", 0, C2_NOT_FOUND, {}, {} },
};

static const ComponentDesc* GetComponentDesc(const std::string& component_name)
{
    const ComponentDesc* result = nullptr;
    for(const auto& desc : g_components_desc) {
        if(component_name == desc.component_name) {
            result = &desc;
            break;
        }
    }
    return result;
}

static std::map<std::string, std::shared_ptr<MfxC2Component>>& GetComponentsCache()
{
    static std::map<std::string, std::shared_ptr<MfxC2Component>> g_components;
    return g_components;
}

static std::shared_ptr<MfxC2Component> GetCachedComponent(const char* name)
{
    std::shared_ptr<MfxC2Component> result;
    auto& components_cache = GetComponentsCache(); // auto& is needed to have ref not a copy of cache

    auto it = components_cache.find(name);
    if(it != components_cache.end()) {
        result = it->second;
    }
    else {
        const ComponentDesc* desc = GetComponentDesc(name);
        ASSERT_NE(desc, nullptr);

        MfxC2Component* mfx_component;
        c2_status_t status = MfxCreateC2Component(name, desc->flags, &mfx_component);
        EXPECT_EQ(status, desc->creation_status);
        if(desc->creation_status == C2_OK) {
            EXPECT_NE(mfx_component, nullptr);
            result = std::shared_ptr<MfxC2Component>(mfx_component);

            components_cache.emplace(name, result);
        }
    }
    return result;
}

// Assures that all decoding components might be successfully created.
// NonExistingDecoder cannot be created and C2_NOT_FOUND error is returned.
TEST(MfxDecoderComponent, Create)
{
    for(const auto& desc : g_components_desc) {

        std::shared_ptr<MfxC2Component> decoder = GetCachedComponent(desc.component_name);

        EXPECT_EQ(decoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;
    }
}

// Checks that all successfully created decoding components expose C2ComponentInterface
// and return correct information once queried (component name).
TEST(MfxDecoderComponent, intf)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        EXPECT_EQ(comp_intf->getName(), desc.component_name);
    } );
}

// Checks list of actually supported parameters by all decoding components.
// Parameters order doesn't matter.
// For every parameter index, name, required and persistent fields are checked.
TEST(MfxDecoderComponent, getSupportedParams)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        std::vector<std::shared_ptr<C2ParamDescriptor>> params_actual;
        c2_status_t sts = comp_intf->getSupportedParams(&params_actual);
        EXPECT_EQ(sts, C2_OK);

        EXPECT_EQ(desc.params_desc.size(), params_actual.size());

        for(const C2ParamDescriptor& param_expected : desc.params_desc) {

            const auto found_actual = std::find_if(params_actual.begin(), params_actual.end(),
                [&] (auto p) { return p->type() == param_expected.type(); } );

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
    std::unique_ptr<C2Work>* work,
    const std::vector<char>& bitstream, bool end_stream, bool header)
{
    *work = std::make_unique<C2Work>();
    C2BufferPack* buffer_pack = &((*work)->input);

    buffer_pack->flags = flags_t(0);
    if (header)
        buffer_pack->flags = flags_t(buffer_pack->flags | BUFFERFLAG_CODEC_CONFIG);
    if (end_stream)
        buffer_pack->flags = flags_t(buffer_pack->flags | BUFFERFLAG_END_OF_STREAM);

    // Set up frame header properties:
    // timestamp is set to correspond to 30 fps stream.
    buffer_pack->ordinal.timestamp = FRAME_DURATION_US * frame_index;
    buffer_pack->ordinal.frame_index = frame_index;
    buffer_pack->ordinal.custom_ordinal = 0;

    do {

        std::shared_ptr<android::C2BlockPool> allocator;
        android::c2_status_t sts = GetC2BlockAllocator(&allocator);

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

        uint8_t* data = nullptr;
        sts = MapLinearBlock(*block, TIMEOUT_NS, &data);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(data, nullptr);

        memcpy(data, &bitstream.front(), bitstream.size());

        C2Event event;
        event.fire(); // pre-fire as buffer is already ready to use
        C2ConstLinearBlock const_block = block->share(0, bitstream.size(), event.fence());
        // make buffer of linear block
        C2BufferData buffer_data = const_block;
        std::shared_ptr<C2Buffer> buffer = std::make_shared<C2Buffer>(buffer_data);

        buffer_pack->buffers.push_back(buffer);

        std::unique_ptr<C2Worklet> worklet = std::make_unique<C2Worklet>();
        // C2 requires 1 allocator per required output item
        worklet->allocators.push_back(allocator);
        // C2 requires output items be allocated in buffers list and set to nulls
        worklet->output.buffers.push_back(nullptr);
        // work of 1 worklet
        (*work)->worklets.push_back(std::move(worklet));
    } while(false);
}

class DecoderConsumer : public C2ComponentListener
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
    void onWorkDone(
        std::weak_ptr<C2Component> component,
        std::vector<std::unique_ptr<C2Work>> workItems) override
    {
        (void)component;

        for(std::unique_ptr<C2Work>& work : workItems) {
            EXPECT_EQ(work->worklets_processed, 1);
            EXPECT_EQ(work->result, C2_OK);

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
            C2BufferPack& buffer_pack = worklet->output;

            uint64_t frame_index = buffer_pack.ordinal.frame_index;

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
                EXPECT_NE(crop.width, 0);
                EXPECT_NE(crop.height, 0);

                std::unique_ptr<const C2GraphicView> c_graph_view;
                sts = MapConstGraphicBlock(*graphic_block, TIMEOUT_NS, &c_graph_view);

                const C2PlanarLayout* layout = c_graph_view->layout();

                const uint8_t* raw  = c_graph_view->data();

                EXPECT_EQ(sts, C2_OK);
                EXPECT_NE(raw, nullptr);

                std::shared_ptr<std::vector<uint8_t>> data_buffer = std::make_shared<std::vector<uint8_t>>();
                data_buffer->resize(crop.width * crop.height * 3 / 2);
                uint8_t* raw_cropped = &(data_buffer->front());
                uint8_t* raw_cropped_chroma = raw_cropped + crop.width * crop.height;
                const uint8_t* raw_chroma = raw + layout->planes[C2PlanarLayout::PLANE_U].mOffset;

                for (uint32_t i = 0; i < crop.height; i++) {
                    const uint32_t stride = layout->planes[C2PlanarLayout::PLANE_Y].rowInc;
                    memcpy(raw_cropped + i * crop.width, raw + (i + crop.top) * stride + crop.left, crop.width);
                }
                for (uint32_t i = 0; i < (crop.height >> 1); i++) {
                    const uint32_t stride = layout->planes[C2PlanarLayout::PLANE_U].rowInc;
                    memcpy(raw_cropped_chroma + i * crop.width, raw_chroma + (i + (crop.top >> 1)) * stride + crop.left, crop.width);
                }

                if(nullptr != raw_cropped) {
                    on_frame_(crop.width, crop.height,
                        raw_cropped, crop.width * crop.height * 3 / 2);
                }
            }
        }
        // if collected all expected frames
        if(frame_submitted_ && (frame_decoded_ == frame_submitted_)) {
            done_.set_value();
        }
    }

    void onTripped(std::weak_ptr<C2Component> component,
                           std::vector<std::shared_ptr<C2SettingResult>> settingResult) override
    {
        (void)component;
        (void)settingResult;
        EXPECT_EQ(true, false) << "onTripped callback shouldn't come";
    }

    void onError(std::weak_ptr<C2Component> component,
                         uint32_t errorCode) override
    {
        (void)component;
        (void)errorCode;
        EXPECT_EQ(true, false) << "onError callback shouldn't come";
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
    component->registerListener(validator);

    C2MemoryTypeSetting setting;
    setting.value = graphics_memory ? C2MemoryTypeGraphics : C2MemoryTypeSystem;

    std::vector<C2Param* const> params = { &setting };
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    std::shared_ptr<C2ComponentInterface> comp_intf = component->intf();

    c2_status_t sts = comp_intf->config_nb(params, &failures);
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
        PrepareWork(frame_index, &work, reader->GetRegionContents(region),
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

    component->unregisterListener(validator);
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

TEST(MfxDecoderComponent, DecodeBitExact)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
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
TEST(MfxDecoderComponent, State)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
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
