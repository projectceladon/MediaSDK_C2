/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_defs.h"
#include <gtest/gtest.h>
#include "test_components.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "C2PlatformSupport.h"
#include "streams/h264/stream_nv12_176x144_cqp_g30_100.264.h"
#include "streams/h264/stream_nv12_352x288_cqp_g15_100.264.h"
#include "streams/h265/stream_nv12_176x144_cqp_g30_100.265.h"
#include "streams/h265/stream_nv12_352x288_cqp_g15_100.265.h"
#include "streams/vp9/stream_nv12_176x144_cqp_g30_100.vp9.ivf.h"
#include "streams/vp9/stream_nv12_352x288_cqp_g15_100.vp9.ivf.h"

#include <future>
#include <set>
#include <queue>

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
    { &stream_nv12_176x144_cqp_g30_100_264 },
    { &stream_nv12_352x288_cqp_g15_100_264 },
    { &stream_nv12_176x144_cqp_g30_100_264, &stream_nv12_352x288_cqp_g15_100_264 },
    { &stream_nv12_352x288_cqp_g15_100_264, &stream_nv12_176x144_cqp_g30_100_264 }
};

static std::vector<std::vector<const StreamDescription*>> h265_streams =
{
    { &stream_nv12_176x144_cqp_g30_100_265 },
    { &stream_nv12_352x288_cqp_g15_100_265 },
    { &stream_nv12_176x144_cqp_g30_100_265, &stream_nv12_352x288_cqp_g15_100_265 },
    { &stream_nv12_352x288_cqp_g15_100_265, &stream_nv12_176x144_cqp_g30_100_265 }
};

static std::vector<std::vector<const StreamDescription*>> vp9_streams =
{
    { &stream_nv12_176x144_cqp_g30_100_vp9_ivf },
    { &stream_nv12_352x288_cqp_g15_100_vp9_ivf },
    { &stream_nv12_176x144_cqp_g30_100_vp9_ivf, &stream_nv12_352x288_cqp_g15_100_vp9_ivf },
    { &stream_nv12_352x288_cqp_g15_100_vp9_ivf, &stream_nv12_176x144_cqp_g30_100_vp9_ivf }
};

static ComponentDesc g_components_desc[] = {
    { "C2.h264vd", 0, C2_OK, dec_params_desc, h264_streams },
    { "C2.h265vd", 0, C2_OK, dec_params_desc, h265_streams },
    { "C2.vp9vd",  0, C2_OK, dec_params_desc, vp9_streams },
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
    const std::vector<char>& bitstream, bool end_stream, bool header, bool complete)
{
    *work = std::make_unique<C2Work>();
    C2FrameData* buffer_pack = &((*work)->input);

    buffer_pack->flags = C2FrameData::flags_t(0);
    // If complete frame do not set C2FrameData::FLAG_CODEC_CONFIG regardless header parameter
    // as it is set when buffer contains header only.
    if (!complete) {
        if (header) {
            buffer_pack->flags = C2FrameData::flags_t(buffer_pack->flags | C2FrameData::FLAG_CODEC_CONFIG);
        } else {
            buffer_pack->flags = C2FrameData::flags_t(buffer_pack->flags | C2FrameData::FLAG_INCOMPLETE);
        }
    }
    if (end_stream)
        buffer_pack->flags = C2FrameData::flags_t(buffer_pack->flags | C2FrameData::FLAG_END_OF_STREAM);

    // Set up frame header properties:
    // timestamp is set to correspond to 30 fps stream.
    buffer_pack->ordinal.timestamp = FRAME_DURATION_US * frame_index;
    buffer_pack->ordinal.frameIndex = frame_index;
    buffer_pack->ordinal.customOrdinal = 0;

    do {

        if (!bitstream.empty()) {
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
        }

        std::unique_ptr<C2Worklet> worklet = std::make_unique<C2Worklet>();
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
    DecoderConsumer(OnFrame on_frame):
        on_frame_(on_frame) {}

    // future ready when validator got all expected frames
    std::future<void> GetFuture()
    {
        return done_.get_future();
    }

    std::string GetMissingFramesList()
    {
        std::lock_guard<std::mutex> lock(expectations_mutex_);
        std::ostringstream ss;
        bool first = true;
        for (const auto& index : frame_expected_set_) {
            if (!first) ss << ";";
            ss << index;
            first = false;
        }
        return ss.str();
    }

    void RegisterSubmittedFrame(uint64_t frame_index, bool expect_empty = false)
    {
        MFX_DEBUG_TRACE_FUNC;
        std::lock_guard<std::mutex> lock(expectations_mutex_);
        if (expect_empty) {
            MFX_DEBUG_TRACE_STREAM("Register empty: " << frame_index);
            frame_expected_empty_set_.insert(frame_index);
        } else {
            MFX_DEBUG_TRACE_STREAM("Register filled: " << frame_index);
            frame_expected_set_.insert(frame_index);
        }
    }

    void ExpectFailures(int count, c2_status_t status)
    {
        std::lock_guard<std::mutex> lock(expectations_mutex_);
        expected_failures_[status] += count;
    }
    // Tests if all works expected to fail actally failed.
    bool FailuresMatch()
    {
        std::lock_guard<std::mutex> lock(expectations_mutex_);
        bool res = true;
        for (auto const& pair : expected_failures_) {
            EXPECT_EQ(pair.second, 0) << "c2_status_t: " << pair.first << ", "
                << "count: " << pair.second;
            if (pair.second != 0) {
                res = false;
            }
        }
        return res;
    }
protected:
    void onWorkDone_nb(
        std::weak_ptr<C2Component> component,
        std::list<std::unique_ptr<C2Work>> workItems) override
    {
        for(std::unique_ptr<C2Work>& work : workItems) {
            EXPECT_EQ(work->worklets.size(), 1u);
            if (C2_OK == work->result) {
                EXPECT_EQ(work->workletsProcessed, 1u);
                if (work->worklets.size() >= 1) {

                    std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
                    C2FrameData& buffer_pack = worklet->output;

                    uint64_t frame_index = buffer_pack.ordinal.frameIndex.peeku();

                    bool last_frame = (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) != 0;
                    EXPECT_EQ(buffer_pack.flags, last_frame ? C2FrameData::FLAG_END_OF_STREAM : C2FrameData::flags_t{});
                    EXPECT_EQ(buffer_pack.ordinal.timestamp, frame_index * FRAME_DURATION_US); // 30 fps

                    c2_status_t sts{};
                    std::unique_ptr<C2ConstGraphicBlock> graphic_block;

                    if (buffer_pack.buffers.size() > 0) {
                        sts = GetC2ConstGraphicBlock(buffer_pack, &graphic_block);
                        EXPECT_EQ(sts, C2_OK) << NAMED(frame_index) << "Output buffer count: " << buffer_pack.buffers.size();
                    }
                    if(nullptr != graphic_block) {

                        C2Rect crop = graphic_block->crop();
                        EXPECT_NE(crop.width, 0u);
                        EXPECT_NE(crop.height, 0u);

                        std::shared_ptr<C2Component> comp = component.lock();
                        if (comp) {
                            C2StreamPictureSizeInfo::output picture_size;
                            sts = comp->intf()->query_vb({&picture_size}, {}, C2_MAY_BLOCK, nullptr);
                            EXPECT_EQ(sts, C2_OK);
                            EXPECT_EQ(picture_size.width, crop.width);
                            EXPECT_EQ(picture_size.height, crop.height);
                            comp = nullptr;
                        }

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
                    {
                        std::lock_guard<std::mutex> lock(expectations_mutex_);
                        bool empty = buffer_pack.buffers.size() == 0;
                        std::set<uint64_t>& check_set = empty ? frame_expected_empty_set_ : frame_expected_set_;
                        EXPECT_EQ(check_set.erase(frame_index), 1u) <<
                            "unexpected " << (empty ? "empty" : "filled") << " frame #" << frame_index;
                        // Signal done_ promise upon completion of all expected frames,
                        // should done under same mutex as expected sets modifications to avoid double set_value
                        if (frame_expected_set_.empty() && frame_expected_empty_set_.empty() &&
                            expected_failures_.empty()) {
                            done_.set_value();
                        }
                    }
                }
            } else {
                std::lock_guard<std::mutex> lock(expectations_mutex_);
                EXPECT_EQ(work->workletsProcessed, 0u);
                EXPECT_TRUE(expected_failures_[work->result] > 0);
                expected_failures_[work->result]--;
                if (expected_failures_[work->result] == 0) {
                    expected_failures_.erase(work->result);
                }

                if (frame_expected_set_.empty() && frame_expected_empty_set_.empty() &&
                    expected_failures_.empty()) {
                    done_.set_value();
                }
            }
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
    std::mutex expectations_mutex_;
    std::set<uint64_t> frame_expected_set_;
    std::set<uint64_t> frame_expected_empty_set_;
    std::map<c2_status_t, int> expected_failures_; // expected failures and how many times they should occur
    std::promise<void> done_;  // fire when all expected frames came
};

// interface to provide flexibility how stream is split to C2 works
class StreamSplitter
{
public:
    virtual ~StreamSplitter() = default;

    virtual bool Read(std::vector<char>* part, bool* header, bool* complete_frame, bool* valid) = 0;

    virtual bool EndOfStream() = 0;
};

// Default StreamSplitter implementation reading by frames
class FrameStreamSplitter : public StreamSplitter
{
private:
    std::unique_ptr<StreamReader> reader;

public:
    FrameStreamSplitter(const std::vector<const StreamDescription*>& streams):
        reader{StreamReader::Create(streams)} {}

    bool Read(std::vector<char>* part, bool* header, bool* complete_frame, bool* valid) override
    {
        StreamDescription::Region region {};
        bool res = reader->Read(StreamReader::Slicing::Frame(), &region, header);
        if (res) {
            *part = reader->GetRegionContents(region);
            *complete_frame = true;
            *valid = true;
        }
        return res;
    }

    bool EndOfStream() override
    {
        return reader->EndOfStream();
    }
};

static void Decode(
    bool graphics_memory,
    std::shared_ptr<C2Component> component,
    std::shared_ptr<DecoderConsumer> validator,
    StreamSplitter* stream_splitter)
{
    c2_blocking_t may_block{C2_MAY_BLOCK};
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

    uint32_t frame_index = 0;

    bool header = false;
    bool complete_frame = false;
    bool valid = false;
    std::vector<char> stream_part;

    while (true) {
        bool res = stream_splitter->Read(&stream_part, &header, &complete_frame, &valid);
        if (!res) break;
        // prepare worklet and push
        std::unique_ptr<C2Work> work;

        // insert input data
        PrepareWork(frame_index, component, &work, stream_part,
            stream_splitter->EndOfStream(), header, complete_frame);
        std::list<std::unique_ptr<C2Work>> works;
        works.push_back(std::move(work));

        if (valid) {
            validator->RegisterSubmittedFrame(frame_index, !complete_frame);
        } else {
            validator->ExpectFailures(1, C2_BAD_VALUE);
        }

        sts = component->queue_nb(&works);
        EXPECT_EQ(sts, C2_OK);

        frame_index++;
    }

    std::future<void> future = validator->GetFuture();
    std::future_status future_sts = future.wait_for(std::chrono::seconds(10));

    EXPECT_EQ(future_sts, std::future_status::ready) << " decoded less frames than expected, missing: "
        << validator->GetMissingFramesList();
    EXPECT_TRUE(validator->FailuresMatch());

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

            SCOPED_TRACE("Stream: " + std::to_string(&streams - desc.streams.data()));

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
                FrameStreamSplitter stream_splitter(streams);

                Decode(use_graphics_memory(i), comp, validator, &stream_splitter);

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

// Decodes streams that caused resolution change,
// supply part of second header, it caused undefined behaviour in mediasdk decoder (264)
// then supply completed header, expects decoder recovers and decodes stream fine.
TEST_P(Decoder, BrokenHeader)
{
    class HeaderBreaker : public FrameStreamSplitter
    {
    public:
        using FrameStreamSplitter::FrameStreamSplitter;
        bool Read(std::vector<char>* part, bool* header, bool* complete_frame, bool* valid) override
        {
            bool res{};
            if (!frame_.empty()) {
                res = true;
                *part = frame_;
                *header = true;
                *complete_frame = true;
                *valid = true;
                frame_.clear();
            } else {
                res = FrameStreamSplitter::Read(part, header, complete_frame, valid);
                if (res) {
                    if (*header) {
                        if (header_index_ > 0) {
                            std::vector<char> contents = *part;
                            *part = std::vector<char>(contents.begin(),
                                std::min(contents.begin() + header_part_len_, contents.end()));
                            *complete_frame = false;
                            *valid = true;
                            frame_ = std::move(contents);
                        }
                        ++header_index_;
                    }
                }
            }
            return res;
        }
    private:
        const int header_part_len_{5};
        int header_index_{0};
        std::vector<char> frame_;
    };

    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        std::map<bool, std::string> memory_names = {
            { false, "(system memory)" },
            { true, "(video memory)" },
        };

        for(const std::vector<const StreamDescription*>& streams : desc.streams) {

            if (streams.size() == 1) continue; // this test demands resolution change

            for (bool use_graphics_memory : { false, true }) {

                CRC32Generator crc_generator;

                GTestBinaryWriter writer(std::ostringstream()
                    << comp_intf->getName() << "-" << GetStreamsCombinedName(streams) << ".nv12");

                DecoderConsumer::OnFrame on_frame = [&] (uint32_t width, uint32_t height,
                    const uint8_t* data, size_t length) {

                    writer.Write(data, length);
                    crc_generator.AddData(width, height, data, length);
                };

                std::shared_ptr<DecoderConsumer> validator =
                    std::make_shared<DecoderConsumer>(on_frame);
                HeaderBreaker stream_splitter(streams);

                Decode(use_graphics_memory, comp, validator, &stream_splitter);

                std::list<uint32_t> actual_crc = crc_generator.GetCrc32();
                std::list<uint32_t> expected_crc;
                std::transform(streams.begin(), streams.end(), std::back_inserter(expected_crc),
                    [] (const StreamDescription* stream) { return stream->crc32_nv12; } );

                EXPECT_EQ(actual_crc, expected_crc) << "Not equal to reference CRC32"
                    << memory_names[use_graphics_memory];
            }
        }
    } );
}

// Default StreamSplitter implementation reading by frames,
// header by nal units
class HeaderSplitter : public StreamSplitter
{
private:
    std::unique_ptr<StreamReader> reader;

    struct Result
    {
        std::vector<char> part;
        bool header;
    };

    std::queue<Result> results;

public:
    HeaderSplitter(const std::vector<const StreamDescription*>& streams):
        reader{StreamReader::Create(streams)} {}

    bool Read(std::vector<char>* part, bool* header, bool* complete_frame, bool* valid) override
    {
        bool res{};
        if (!results.empty()) {
            *part = results.front().part;
            *header = results.front().header;
            *valid = true;
            res = true;
            results.pop();
            *complete_frame = results.empty();
        } else {
            StreamDescription::Region region {};
            size_t prev_pos = reader->GetPos();
            res = reader->Read(StreamReader::Slicing::Frame(), &region, header);
            if (res) {
                if (header) { // split header by NalUnits
                    size_t next_pos = reader->GetPos();
                    res = reader->Seek(prev_pos);

                    if (res) {

                        while (reader->GetPos() < next_pos) {

                            res = reader->Read(StreamReader::Slicing::NalUnit(), &region, header);
                            if (res) {
                                results.push({reader->GetRegionContents(region), *header});

                            } else {
                                break;
                            }
                        }
                        if (!results.empty()) {
                            *part = results.front().part;
                            *header = results.front().header;
                            *valid = true;
                         res = true;
                            results.pop();
                            *complete_frame = results.empty();

                        } else {
                            res = false;
                        }
                    }
                } else {
                    *part = reader->GetRegionContents(region);
                    *header = false;
                    *complete_frame = true;
                    *valid = true;
                 res = true;
                }
            }
        }
        return res;
    }

    bool EndOfStream() override
    {
        return !results.empty() ? false : reader->EndOfStream();
    }
};

// Checks that concatenation chunks read by HeaderSplitter
// is equal to the whole stream.
TEST_P(Decoder, HeaderSplitterReadEverything)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr) {

        if (std::string(desc.component_name) == "C2.vp9vd") return;

        auto append_vector = [](const std::vector<char>& src, std::vector<char>* dst) {
            dst->reserve(dst->size() + src.size());
            std::copy(src.begin(), src.end(), std::back_inserter(*dst));
        };

        for(const std::vector<const StreamDescription*>& streams : desc.streams) {

            HeaderSplitter headerSplitter(streams);
            std::vector<char> split_contents;

            do {
                std::vector<char> part;
                bool header;
                bool complete_frame;
                bool valid;
                bool res = headerSplitter.Read(&part, &header, &complete_frame, &valid);
                EXPECT_TRUE(res);
                EXPECT_TRUE(valid);
                EXPECT_NE(part.size(), 0u);

                append_vector(part, &split_contents);

            } while (!headerSplitter.EndOfStream());

            std::vector<char> original_contents;

            for (const auto& stream : streams) {
                append_vector(stream->data, &original_contents);
            }

            EXPECT_EQ(original_contents.size(), split_contents.size());
            EXPECT_EQ(original_contents, split_contents);
        }
    } );
}

// Sends streams for decoding emulating C2 runtime behaviour:
// if frame contains header, the frame is sent split by NAL units.
TEST_P(Decoder, SeparateHeaders)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        if (std::string(desc.component_name) == "C2.vp9vd") return;

        std::map<bool, std::string> memory_names = {
            { false, "(system memory)" },
            { true, "(video memory)" },
        };

        for(const std::vector<const StreamDescription*>& streams : desc.streams) {

            SCOPED_TRACE("Stream: " + std::to_string(&streams - desc.streams.data()));

            for (bool use_graphics_memory : { false, true }) {

                CRC32Generator crc_generator;

                GTestBinaryWriter writer(std::ostringstream()
                    << comp_intf->getName() << "-" << GetStreamsCombinedName(streams) << ".nv12");

                DecoderConsumer::OnFrame on_frame = [&] (uint32_t width, uint32_t height,
                    const uint8_t* data, size_t length) {

                    writer.Write(data, length);
                    crc_generator.AddData(width, height, data, length);
                };

                std::shared_ptr<DecoderConsumer> validator =
                    std::make_shared<DecoderConsumer>(on_frame);

                HeaderSplitter headerSplitter(streams);
                Decode(use_graphics_memory, comp, validator, &headerSplitter);

                std::list<uint32_t> actual_crc = crc_generator.GetCrc32();
                std::list<uint32_t> expected_crc;
                std::transform(streams.begin(), streams.end(), std::back_inserter(expected_crc),
                    [] (const StreamDescription* stream) { return stream->crc32_nv12; } );

                EXPECT_EQ(actual_crc, expected_crc) << "Not equal to reference CRC32"
                    << memory_names[use_graphics_memory];
            }
        }
    } );
}

// Sends last frame without eos flag, then empty input buffer with eos flag.
TEST_P(Decoder, SeparateEos)
{
    class HeaderBreaker : public FrameStreamSplitter
    {
    public:
        using FrameStreamSplitter::FrameStreamSplitter;
        bool Read(std::vector<char>* part, bool* header, bool* complete_frame, bool* valid) override
        {
            bool res{};
            if (FrameStreamSplitter::EndOfStream()) {
                part->clear();
                *header = false;
                *complete_frame = false;
                *valid = true;
                res = !eos_returned_;
                eos_returned_ = true;
            } else {
                res = FrameStreamSplitter::Read(part, header, complete_frame, valid);
            }
            return res;
        }
        bool EndOfStream() override
        {
            return eos_returned_;
        }
    private:
        bool eos_returned_{false};
    };

    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        std::map<bool, std::string> memory_names = {
            { false, "(system memory)" },
            { true, "(video memory)" },
        };

        for(const std::vector<const StreamDescription*>& streams : desc.streams) {

            for (bool use_graphics_memory : { false, true }) {

                CRC32Generator crc_generator;

                GTestBinaryWriter writer(std::ostringstream()
                    << comp_intf->getName() << "-" << GetStreamsCombinedName(streams) << ".nv12");

                DecoderConsumer::OnFrame on_frame = [&] (uint32_t width, uint32_t height,
                    const uint8_t* data, size_t length) {

                    writer.Write(data, length);
                    crc_generator.AddData(width, height, data, length);
                };

                std::shared_ptr<DecoderConsumer> validator =
                    std::make_shared<DecoderConsumer>(on_frame);
                HeaderBreaker stream_splitter(streams);

                Decode(use_graphics_memory, comp, validator, &stream_splitter);

                std::list<uint32_t> actual_crc = crc_generator.GetCrc32();
                std::list<uint32_t> expected_crc;
                std::transform(streams.begin(), streams.end(), std::back_inserter(expected_crc),
                    [] (const StreamDescription* stream) { return stream->crc32_nv12; } );

                EXPECT_EQ(actual_crc, expected_crc) << "Not equal to reference CRC32"
                    << memory_names[use_graphics_memory];
            }
        }
    } );
}

// Follow last frame with series of Eos works without frame.
TEST_P(Decoder, MultipleEos)
{
    class EosBreaker : public FrameStreamSplitter
    {
    public:
        using FrameStreamSplitter::FrameStreamSplitter;
        bool Read(std::vector<char>* part, bool* header, bool* complete_frame, bool* valid) override
        {
            bool res{};
            if (FrameStreamSplitter::EndOfStream()) {
                part->clear();
                *header = false;
                *complete_frame = false;
                *valid = (eos_returned_ == 0);
                res = (eos_returned_ < 10);
                eos_returned_++;
            } else {
                res = FrameStreamSplitter::Read(part, header, complete_frame, valid);
            }
            return res;
        }
        bool EndOfStream() override
        {
            return eos_returned_ > 0;
        }
    private:
        int eos_returned_{0};
    };

    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        std::map<bool, std::string> memory_names = {
            { false, "(system memory)" },
            { true, "(video memory)" },
        };

        for(const std::vector<const StreamDescription*>& streams : desc.streams) {

            for (bool use_graphics_memory : { false, true }) {

                CRC32Generator crc_generator;

                GTestBinaryWriter writer(std::ostringstream()
                    << comp_intf->getName() << "-" << GetStreamsCombinedName(streams) << ".nv12");

                DecoderConsumer::OnFrame on_frame = [&] (uint32_t width, uint32_t height,
                    const uint8_t* data, size_t length) {

                    writer.Write(data, length);
                    crc_generator.AddData(width, height, data, length);
                };

                std::shared_ptr<DecoderConsumer> validator =
                    std::make_shared<DecoderConsumer>(on_frame);
                EosBreaker stream_splitter(streams);

                Decode(use_graphics_memory, comp, validator, &stream_splitter);

                std::list<uint32_t> actual_crc = crc_generator.GetCrc32();
                std::list<uint32_t> expected_crc;
                std::transform(streams.begin(), streams.end(), std::back_inserter(expected_crc),
                    [] (const StreamDescription* stream) { return stream->crc32_nv12; } );

                EXPECT_EQ(actual_crc, expected_crc) << "Not equal to reference CRC32"
                    << memory_names[use_graphics_memory];
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
