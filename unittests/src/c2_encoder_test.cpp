// Copyright (c) 2017-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_c2_defs.h"
#include <gtest/gtest.h>
#include <C2Config.h>
#include "test_components.h"
#include "test_streams.h"
#include "test_params.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "C2PlatformSupport.h"

#include <set>
#include <future>
#include <iostream>
#include <fstream>

using namespace android;

const unsigned int SINGLE_STREAM_ID = 0u;
const float FRAME_RATE = 30.0; // 30 fps
const uint64_t FRAME_DURATION_US = (uint64_t)(1000000 / FRAME_RATE);
// Low res is chosen to speed up the tests.
const uint32_t MIN_W = 176;
const uint32_t MIN_H = 144;
const uint32_t MAX_W = 4096;
const uint32_t MAX_H = 4096;
const uint32_t FRAME_WIDTH = 320;
const uint32_t FRAME_HEIGHT = 240;

const uint32_t FRAME_FORMAT = MFX_FOURCC_NV12; // fourcc nv12
// This frame count is required by StaticBitrate test, the encoder cannot follow
// bitrate on shorter frame sequences.
const uint32_t FRAME_COUNT = 150; // 10 default GOP size
const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

static std::vector<C2ParamDescriptor> h264_params_desc = 
{
    { false, C2_PARAMKEY_COMPONENT_NAME, C2ComponentNameSetting::PARAM_TYPE },
    { false, C2_PARAMKEY_COMPONENT_KIND, C2ComponentKindSetting::PARAM_TYPE },
    { false, C2_PARAMKEY_COMPONENT_DOMAIN, C2ComponentDomainSetting::PARAM_TYPE },
    { false, C2_PARAMKEY_PICTURE_SIZE, C2StreamPictureSizeInfo::input::PARAM_TYPE },
    { false, C2_PARAMKEY_INPUT_MEDIA_TYPE, C2PortMediaTypeSetting::input::PARAM_TYPE },
    { false, C2_PARAMKEY_OUTPUT_MEDIA_TYPE, C2PortMediaTypeSetting::output::PARAM_TYPE },
    { false, C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE, C2StreamBufferTypeSetting::input::PARAM_TYPE },
    { false, C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE, C2StreamBufferTypeSetting::output::PARAM_TYPE },
    { false, C2_PARAMKEY_BITRATE_MODE, C2StreamBitrateModeTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_BITRATE, C2StreamBitrateInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_FRAME_RATE, C2StreamFrameRateInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_GOP, C2StreamGopTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_REQUEST_SYNC_FRAME, C2StreamRequestSyncFrameTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_SYNC_FRAME_INTERVAL, C2StreamSyncFrameIntervalTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_PROFILE_LEVEL, C2StreamProfileLevelInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_COLOR_ASPECTS, C2StreamColorAspectsInfo::input::PARAM_TYPE },
    { false, C2_PARAMKEY_VUI_COLOR_ASPECTS, C2StreamColorAspectsInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_INTRA_REFRESH, C2StreamIntraRefreshTuning::output::PARAM_TYPE },
};

static std::vector<C2ParamDescriptor> h265_params_desc = 
{
    { false, C2_PARAMKEY_COMPONENT_NAME, C2ComponentNameSetting::PARAM_TYPE },
    { false, C2_PARAMKEY_COMPONENT_KIND, C2ComponentKindSetting::PARAM_TYPE },
    { false, C2_PARAMKEY_COMPONENT_DOMAIN, C2ComponentDomainSetting::PARAM_TYPE },
    { false, C2_PARAMKEY_PICTURE_SIZE, C2StreamPictureSizeInfo::input::PARAM_TYPE },
    { false, C2_PARAMKEY_INPUT_MEDIA_TYPE, C2PortMediaTypeSetting::input::PARAM_TYPE },
    { false, C2_PARAMKEY_OUTPUT_MEDIA_TYPE, C2PortMediaTypeSetting::output::PARAM_TYPE },
    { false, C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE, C2StreamBufferTypeSetting::input::PARAM_TYPE },
    { false, C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE, C2StreamBufferTypeSetting::output::PARAM_TYPE },
    { false, C2_PARAMKEY_BITRATE_MODE, C2StreamBitrateModeTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_BITRATE, C2StreamBitrateInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_FRAME_RATE, C2StreamFrameRateInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_GOP, C2StreamGopTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_REQUEST_SYNC_FRAME, C2StreamRequestSyncFrameTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_SYNC_FRAME_INTERVAL, C2StreamSyncFrameIntervalTuning::output::PARAM_TYPE },
    { false, C2_PARAMKEY_PROFILE_LEVEL, C2StreamProfileLevelInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_PIXEL_FORMAT, C2StreamPixelFormatInfo::input::PARAM_TYPE },
    { false, C2_PARAMKEY_COLOR_ASPECTS, C2StreamColorAspectsInfo::input::PARAM_TYPE },
    { false, C2_PARAMKEY_VUI_COLOR_ASPECTS, C2StreamColorAspectsInfo::output::PARAM_TYPE },
    { false, C2_PARAMKEY_INTRA_REFRESH, C2StreamIntraRefreshTuning::output::PARAM_TYPE },
};

namespace {

    struct ComponentDesc
    {
        const char* component_name;
        MfxC2Component::CreateConfig config;
        c2_status_t creation_status;
        std::vector<C2ParamDescriptor> params_desc;
        C2ParamValues default_values;
        c2_status_t query_status;
        std::vector<C2ProfileLevelStruct> profile_levels;
        uint32_t four_cc;

        typedef bool TestStreamProfileLevel(
            const C2ProfileLevelStruct& profile_level, std::vector<char>&& stream, std::string* message);
        TestStreamProfileLevel* test_stream_profile_level;
    };

    // This function is used by ::testing::PrintToStringParamName to give
    // parameterized tests reasonable names instead of /1, /2, ...
    void PrintTo(const ComponentDesc& desc, ::std::ostream* os)
    {
        PrintAlphaNumeric(desc.component_name, os);
    }

    class CreateEncoder : public ::testing::TestWithParam<ComponentDesc>
    {
    };
    // Test fixture class called Encoder to beautify output
    class Encoder : public ::testing::TestWithParam<ComponentDesc>
    {
    };
}

static C2ParamValues GetDefaultParamValues(const char* component_name)
{
    C2ParamValues default_values;

    default_values.Append(new C2StreamPictureSizeInfo::input(SINGLE_STREAM_ID, MIN_W, MIN_H));
    default_values.Append(new C2StreamBitrateModeTuning::output(SINGLE_STREAM_ID, C2Config::BITRATE_VARIABLE));
    default_values.Append(new C2StreamBitrateInfo::output(SINGLE_STREAM_ID, 64000));
    default_values.Append(new C2StreamFrameRateInfo::output(SINGLE_STREAM_ID, 1.));
    default_values.AppendFlex(C2StreamGopTuning::output::AllocUnique(0/* flexCount */, SINGLE_STREAM_ID));
    default_values.Append(new C2StreamRequestSyncFrameTuning::output(SINGLE_STREAM_ID, C2_FALSE));
    default_values.Append(new C2StreamSyncFrameIntervalTuning::output(SINGLE_STREAM_ID, 1000000));
    default_values.Append(new C2StreamColorAspectsInfo::input(SINGLE_STREAM_ID, 
                                            C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED));
    default_values.Append(new C2StreamColorAspectsInfo::output(SINGLE_STREAM_ID, 
                                            C2Color::RANGE_LIMITED, C2Color::PRIMARIES_UNSPECIFIED,
                                            C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED));
    default_values.Append(new C2StreamIntraRefreshTuning::output(0u, C2Config::INTRA_REFRESH_DISABLED, 0.));

    if (!strcmp(component_name, "c2.intel.avc.encoder")) {
        default_values.Append(new C2StreamProfileLevelInfo::output(
                    SINGLE_STREAM_ID, PROFILE_AVC_CONSTRAINED_BASELINE, LEVEL_AVC_5_2));
    } else if (!strcmp(component_name, "c2.intel.hevc.encoder")) {
        default_values.Append(new C2StreamProfileLevelInfo::output(
                    SINGLE_STREAM_ID, PROFILE_HEVC_MAIN, LEVEL_HEVC_MAIN_6));
        default_values.Append(new C2StreamPixelFormatInfo::input(0u, HAL_PIXEL_FORMAT_YCBCR_420_888));
    }

    return default_values;
}

static ComponentDesc NonExistingEncoderDesc()
{
    ComponentDesc desc {};
    desc.component_name = "c2.intel.missing.encoder";
    desc.creation_status = C2_NOT_FOUND;
    return desc;
}

static ComponentDesc g_components_desc[] = {
    { "c2.intel.avc.encoder", 
        MfxC2Component::CreateConfig{
                .concurrent_instances=12, }, 
        C2_OK, 
        h264_params_desc, 
        GetDefaultParamValues("c2.intel.avc.encoder"), 
        /*C2_CORRUPTED*/C2_OK,
        { g_h264_profile_levels, g_h264_profile_levels + g_h264_profile_levels_count }, 
        MFX_CODEC_AVC,
        &TestAvcStreamProfileLevel },
    { "c2.intel.hevc.encoder", 
        MfxC2Component::CreateConfig{
                    .concurrent_instances=12, }, 
        C2_OK, 
        h265_params_desc, 
        GetDefaultParamValues("c2.intel.hevc.encoder"), 
        /*C2_CORRUPTED*/C2_OK,
        { g_h265_profile_levels, g_h265_profile_levels + g_h265_profile_levels_count }, 
        MFX_CODEC_HEVC,
        &TestHevcStreamProfileLevel },
};

static ComponentDesc g_invalid_components_desc[] = {
    NonExistingEncoderDesc(),
};

// Assures that all encoding components might be successfully created.
// NonExistingEncoder cannot be created and C2_NOT_FOUND error is returned.
TEST_P(CreateEncoder, Create)
{
    const ComponentDesc& desc = GetParam();

    std::shared_ptr<MfxC2Component> encoder = GetCachedComponent(desc);

    EXPECT_EQ(encoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;
}

// Checks that all successfully created encoding components expose C2ComponentInterface
// and return correct information once queried (component name).
TEST_P(Encoder, intf)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        EXPECT_EQ(comp_intf->getName(), desc.component_name);
    } );
}

static void PrepareWork(uint32_t frame_index, bool last_frame, bool graphics_memory,
    std::shared_ptr<const C2Component> component,
    std::unique_ptr<C2Work>* work,
    const std::vector<FrameGenerator*>& generators)
{
    *work = std::make_unique<C2Work>();
    C2FrameData* buffer_pack = &((*work)->input);

    if (!last_frame) {
        buffer_pack->flags = C2FrameData::flags_t(0);
    } else {
        buffer_pack->flags = C2FrameData::FLAG_END_OF_STREAM;
    }

    // Set up frame header properties:
    // timestamp is set to correspond to 30 fps stream.
    buffer_pack->ordinal.timestamp = FRAME_DURATION_US * frame_index;
    buffer_pack->ordinal.frameIndex = frame_index;
    buffer_pack->ordinal.customOrdinal = 0;

    do {
        std::shared_ptr<C2BlockPool> allocator;
        c2_status_t sts = GetCodec2BlockPool(C2BlockPool::BASIC_GRAPHIC,
            component, &allocator);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(allocator, nullptr);

        if(nullptr == allocator) break;

        C2MemoryUsage mem_usage = {
            graphics_memory ? android::C2AndroidMemoryUsage::HW_CODEC_READ : C2MemoryUsage::CPU_READ,
            C2MemoryUsage::CPU_WRITE
        };

        std::shared_ptr<C2GraphicBlock> block;
        sts = allocator->fetchGraphicBlock(FRAME_WIDTH, FRAME_HEIGHT,
                    MfxFourCCToGralloc(FRAME_FORMAT, graphics_memory),
                    mem_usage, &block);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(block, nullptr);

        if(nullptr == block) break;

        {
            std::unique_ptr<C2GraphicView> graph_view;
            sts = MapGraphicBlock(*block, TIMEOUT_NS, &graph_view);
            EXPECT_EQ(sts, C2_OK);
            EXPECT_TRUE(graph_view);

            if (graph_view) {
                const C2PlanarLayout layout = graph_view->layout();
                EXPECT_EQ(layout.type, C2PlanarLayout::TYPE_YUV);

                uint8_t* const* data = graph_view->data();
                EXPECT_NE(data, nullptr);

                for (uint32_t i = 0; i < layout.numPlanes; ++i) {
                    EXPECT_NE(data[i], nullptr);
                }

                const uint32_t stride = layout.planes[C2PlanarLayout::PLANE_Y].rowInc;
                const uint32_t alloc_height =
                    (data[C2PlanarLayout::PLANE_U] - data[C2PlanarLayout::PLANE_Y]) / stride;
                const size_t frame_size = stride * alloc_height * 3 / 2;

                // Allocate frame in system memory, generate contents there, copy to gpu memory
                // as direct write per pixel is very slow.
                std::vector<uint8_t> frame(frame_size);

                for(FrameGenerator* generator : generators) {
                    generator->Apply(frame_index, frame.data(), FRAME_WIDTH, stride, alloc_height);
                }
                std::copy(frame.begin(), frame.end(), data[C2PlanarLayout::PLANE_Y]);
            }
        }

        // C2Event event; // not supported yet, left for future use
        // event.fire(); // pre-fire as buffer is already ready to use
        C2ConstGraphicBlock const_block = block->share(block->crop(), C2Fence()/*event.fence()*/);
        // make buffer of graphic block
        std::shared_ptr<C2Buffer> buffer = std::make_shared<C2Buffer>(MakeC2Buffer( { const_block } ));

        buffer_pack->buffers.push_back(buffer);

        std::unique_ptr<C2Worklet> worklet = std::make_unique<C2Worklet>();
        // work of 1 worklet
        (*work)->worklets.push_back(std::move(worklet));
    } while(false);
}

class EncoderConsumer : public C2Component::Listener
{
public:
    typedef std::function<void(const C2Worklet& worklet, const uint8_t* data, size_t length)> OnFrame;

public:
    EncoderConsumer(
        OnFrame on_frame,
        uint64_t frame_count = FRAME_COUNT,
        std::set<uint64_t>&& empty_frames = {}):

        on_frame_(on_frame),
        expected_empty_frames_(std::move(empty_frames))
    {   // fill expected_filled_frames_ with those not in expected_empty_frames_
        for (uint64_t i = 0; i < frame_count; ++i) {
            if (expected_empty_frames_.find(i) == expected_empty_frames_.end()) {
                expected_filled_frames_.push_back(i);
            }
        }
    }

    // future ready when validator got all expected frames
    std::future<void> GetFuture()
    {
        return done_.get_future();
    }

protected:
    void onWorkDone_nb(
        std::weak_ptr<C2Component> component,
        std::list<std::unique_ptr<C2Work>> workItems) override
    {
        (void)component;

        for(std::unique_ptr<C2Work>& work : workItems) {
            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
            EXPECT_NE(nullptr, worklet);
            if(nullptr == worklet) continue;

            C2FrameData& buffer_pack = worklet->output;

            uint64_t frame_index = buffer_pack.ordinal.frameIndex.peeku();

            EXPECT_EQ(work->workletsProcessed, 1u) << NAMED(frame_index);
            EXPECT_EQ(work->result, C2_OK) << NAMED(frame_index);
            EXPECT_EQ(buffer_pack.ordinal.timestamp, frame_index * FRAME_DURATION_US); // 30 fps

            if (buffer_pack.buffers.size() != 0) {
                {
                    std::lock_guard<std::mutex> lock(expectations_mutex_);
                    // expect filled frame is first of expected filled frames - check their order
                    if (!expected_filled_frames_.empty() && expected_filled_frames_.front() == frame_index) {
                        expected_filled_frames_.pop_front();
                    } else {
                        ADD_FAILURE() << "unexpected filled: " << frame_index;
                    }
                }
                std::unique_ptr<C2ConstLinearBlock> linear_block;
                c2_status_t sts = GetC2ConstLinearBlock(buffer_pack, &linear_block);
                EXPECT_EQ(sts, C2_OK) << frame_index;

                if(nullptr != linear_block) {

                    std::unique_ptr<C2ReadView> read_view;
                    sts = MapConstLinearBlock(*linear_block, TIMEOUT_NS, &read_view);
                    EXPECT_EQ(sts, C2_OK);
                    EXPECT_NE(read_view, nullptr);

                    if (nullptr != read_view) {
                        const uint8_t* raw  = read_view->data();
                        EXPECT_NE(raw, nullptr);
                        EXPECT_NE(linear_block->size(), 0u);

                        if(nullptr != raw) {
                            on_frame_(*worklet, raw + linear_block->offset(), linear_block->size());
                        }
                    }
                }
            } else {
                std::lock_guard<std::mutex> lock(expectations_mutex_);
                // check empty frame is just in expected set - no order checking
                size_t erased_count = expected_empty_frames_.erase(frame_index);
                EXPECT_EQ(erased_count, 1) << "unexpected empty: " << frame_index;
            }
        }
        {
            std::lock_guard<std::mutex> lock(expectations_mutex_);
            // if collected all expected frames
            if (expected_empty_frames_.size() == 0 && expected_filled_frames_.size() == 0) {
                done_.set_value();
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

    std::list<uint64_t> expected_filled_frames_;
    std::set<uint64_t> expected_empty_frames_;
    std::promise<void> done_; // fire when all expected frames came
};

typedef std::function<void (uint32_t frame_index, C2Work* work)> BeforeQueueWork;

static void Encode(
    uint32_t frame_count, bool graphics_memory,
    std::shared_ptr<C2Component> component,
    std::shared_ptr<EncoderConsumer> validator,
    const std::vector<FrameGenerator*>& generators,
    BeforeQueueWork before_queue_work = {})
{
    c2_blocking_t may_block {};
    component->setListener_vb(validator, may_block);

    c2_status_t sts = component->start();
    EXPECT_EQ(sts, C2_OK);

    for(uint32_t frame_index = 0; frame_index < frame_count; ++frame_index) {
        // prepare worklet and push
        std::unique_ptr<C2Work> work;

        // insert input data
        PrepareWork(frame_index, frame_index == frame_count - 1, graphics_memory,
            component, &work, generators);
        if (before_queue_work) {
            before_queue_work(frame_index, work.get());
        }
        std::list<std::unique_ptr<C2Work>> works;
        works.push_back(std::move(work));

        sts = component->queue_nb(&works);
        EXPECT_EQ(sts, C2_OK);
    }

    std::future<void> future = validator->GetFuture();
    std::future_status future_sts = future.wait_for(std::chrono::seconds(10));
    EXPECT_EQ(future_sts, std::future_status::ready) << " encoded less frames than expected";

    component->setListener_vb(nullptr, may_block);
    sts = component->stop();
    EXPECT_EQ(sts, C2_OK);
}

struct EncoderListener : public C2Component::Listener
{
    std::function<void(const std::unique_ptr<C2Work>& work)> on_work_done_;

    EncoderListener(std::function<void(const std::unique_ptr<C2Work>& work)> on_work_done):
        on_work_done_(on_work_done) {}

    void onWorkDone_nb(std::weak_ptr<C2Component>,
        std::list<std::unique_ptr<C2Work>> workItems) override
    {
        for (auto const& work : workItems) {
            on_work_done_(work);
        }
    }

    void onTripped_nb(std::weak_ptr<C2Component>,
        std::vector<std::shared_ptr<C2SettingResult>>) override {};
    void onError_nb(std::weak_ptr<C2Component>, uint32_t) override {};

};

// Encodes streams multiple times on the same encoder instance.
// While encoding, stops when gets first output,
// then starts again.
// Decodes till the end on last pass though.
TEST_P(Encoder, StopWhileEncoding)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr) {

        const int REPEATS_COUNT = 3;
        std::set<c2_status_t> status_set_; // gather statuses for all passes

        for (int i = 0; i < REPEATS_COUNT; ++i) {
            std::atomic<bool> got_work_{false};

            auto on_work_done = [&](const std::unique_ptr<C2Work>& work) {
                status_set_.insert(work->result);
                got_work_ = true;
            };

            comp->setListener_vb(std::make_shared<EncoderListener>(on_work_done), C2_MAY_BLOCK);

            StripeGenerator stripe_generator;

            EXPECT_EQ(comp->start(), C2_OK);

            for(uint32_t frame_index = 0; frame_index < FRAME_COUNT; ++frame_index) {

                // if pass is not last stop queueing
                if (i != (REPEATS_COUNT - 1) && got_work_) break;

                std::unique_ptr<C2Work> work;
                PrepareWork(frame_index, frame_index == (FRAME_COUNT - 1), false,
                        comp, &work, { &stripe_generator });

                std::list<std::unique_ptr<C2Work>> works;
                works.push_back(std::move(work));
                EXPECT_EQ(comp->queue_nb(&works), C2_OK);
            }

            EXPECT_EQ(comp->stop(), C2_OK);

            std::set<c2_status_t> expected_status_set{ C2_OK };
            EXPECT_EQ(status_set_, expected_status_set);
        }
    });
}

// Checks the correctness of all encoding components state machine.
// The component should be able to start from STOPPED (initial) state,
// stop from RUNNING state. Otherwise, C2_BAD_STATE should be returned.
TEST_P(Encoder, State)
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

// Checks list of actually supported parameters by all encoding components.
// Parameters order doesn't matter.
// For every parameter index, name, required and persistent fields are checked.
TEST_P(Encoder, getSupportedParams)
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

uint32_t CountIdrSlices(std::vector<char>&& contents, const char* component_name)
{
    StreamDescription stream {};
    stream.data = std::move(contents); // do not init sps/pps regions, don't care of them

    SingleStreamReader reader(&stream);

    uint32_t count = 0;

    StreamDescription::Region region {};
    bool header {};
    size_t start_code_len {};
    while (reader.Read(StreamReader::Slicing::NalUnit(), &region, &header, &start_code_len)) {

        if (region.size > start_code_len) {
            char header_byte = stream.data[region.offset + start_code_len]; // first byte start code
            uint8_t nal_unit_type = 0;
            if (!strcmp(component_name, "c2.intel.avc.encoder")) {
                nal_unit_type = (uint8_t)header_byte & 0x1F;
                const uint8_t IDR_SLICE = 5;
                if (nal_unit_type == IDR_SLICE) {
                    ++count;
                }
            } else if (!strcmp(component_name, "c2.intel.hevc.encoder")) {
                nal_unit_type = ((uint8_t)header_byte & 0x7E) >> 1; // extract 6 bits: from 2nd to 7th
                const uint8_t IDR_W_RADL = 19;
                const uint8_t IDR_N_LP = 20;
                if ((nal_unit_type == IDR_W_RADL) || (nal_unit_type == IDR_N_LP))
                    ++count;
            }
        }
    }
    return count;
}

bool CheckFrameRateInStream(std::vector<char>&& contents, const float expected,  const char* component_name, std::string* message)
{
    std::ostringstream oss;
    bool res = false;
    if (!strcmp(component_name, "c2.intel.avc.encoder")) {
        HeaderParser::AvcSequenceParameterSet sps;
        if (sps.ExtractSequenceParameterSet(std::move(contents))) {
            if (abs(sps.frame_rate_ - expected) > 0.001) { //FrameRate setting keep 3 sign after dot
                oss << "ERR: Wrong FrameRate in stream" << std::endl << "Expected: " << expected << " Actual: " << sps.frame_rate_ << std::endl;
            } else {
                res = true;
            }
        } else {
            oss << "sps is not found in bitstream" << std::endl;
        }
    } else if (!strcmp(component_name, "c2.intel.hevc.encoder")) {
        HeaderParser::HevcSequenceParameterSet sps;
        if (sps.ExtractSequenceParameterSet(std::move(contents))) {
            if (abs(sps.frame_rate_ - expected) > 0.001) { //FrameRate setting keep 3 sign after dot
                oss << "ERR: Wrong FrameRate in stream" << std::endl << "Expected: " << expected << " Actual: " << sps.frame_rate_ << std::endl;
            } else {
                res = true;
            }
        } else {
            oss << "sps is not found in bitstream" << std::endl;
        }
    } else {
        res = false;
        oss << "ERR: unknown codec" << std::endl;
    }
    *message = oss.str();
    return res;
}

static std::vector<char> ExtractHeader(std::vector<char>&& bitstream, uint32_t four_cc)
{
    EXPECT_TRUE(four_cc == MFX_CODEC_AVC || four_cc == MFX_CODEC_HEVC);

    const uint8_t UNIT_TYPE_SPS = (four_cc == MFX_CODEC_AVC) ? 7 : 33;
    const uint8_t UNIT_TYPE_PPS = (four_cc == MFX_CODEC_AVC) ? 8 : 34;
    const uint8_t UNIT_TYPE_VPS = 32; // used for HEVC only

    std::vector<char> sps;
    std::vector<char> pps;
    std::vector<char> vps;

    StreamDescription stream{};
    stream.data = std::move(bitstream); // do not init sps/pps regions, don't care of them
    SingleStreamReader reader(&stream);
    StreamDescription::Region region{};
    bool header{};
    size_t start_code_len{};

    while (reader.Read(StreamReader::Slicing::NalUnit(), &region, &header, &start_code_len)) {
        if (region.size > start_code_len) {
            char header_byte = stream.data[region.offset + start_code_len];
            uint8_t nal_unit_type = (four_cc == MFX_CODEC_AVC) ?
                (uint8_t)header_byte & 0x1F :
                ((uint8_t)header_byte & 0x7E) >> 1;

            if (nal_unit_type == UNIT_TYPE_SPS) {
                sps = reader.GetRegionContents(region);
            } else if (nal_unit_type == UNIT_TYPE_PPS) {
                pps = reader.GetRegionContents(region);
            } else if (nal_unit_type == UNIT_TYPE_VPS) {
                vps = reader.GetRegionContents(region);
            }
        }
    }
    EXPECT_NE(sps.size(), 0u);
    EXPECT_NE(pps.size(), 0u);
    if (four_cc != MFX_CODEC_AVC)
        EXPECT_NE(vps.size(), 0u);

    std::vector<char> res;
    if (four_cc != MFX_CODEC_AVC)
        res = std::move(vps);
    res.insert(res.end(), sps.begin(), sps.end()); // concatenate
    res.insert(res.end(), pps.begin(), pps.end());

    return res;
}

// Tests that header (vps + sps + pps) is supplied with C2StreamInitDataInfo::output
// through C2Worklet::output::configUpdate.
// Checks if C2StreamInitDataInfo::output contents is the same as vps + sps + pps from encoded stream.
TEST_P(Encoder, EncodeHeaderSupplied)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& desc, C2CompPtr comp, C2CompIntfPtr) {

        StripeGenerator stripe_generator;

        int header_update_count = 0;

        EncoderConsumer::OnFrame on_frame =
            [&] (const C2Worklet& worklet, const uint8_t* data, size_t length) {

            const auto& update = worklet.output.configUpdate;
            const auto& it = std::find_if(update.begin(), update.end(), [](const auto& p) {
                return p->type() == C2Param::Type(C2StreamInitDataInfo::output::PARAM_TYPE);
            });

            if (it != update.end() && *it) {

                const C2StreamInitDataInfo::output* csd_info = (const C2StreamInitDataInfo::output*)it->get();

                ++header_update_count;

                EXPECT_EQ(csd_info->stream(), 0u);

                std::vector<char> frame_contents((const char*)data, (const char*)data + length);
                std::vector<char> read_header = ExtractHeader(std::move(frame_contents), desc.four_cc);

                EXPECT_EQ(csd_info->flexCount(), read_header.size());

                size_t compare_len = std::min(csd_info->flexCount(), read_header.size());
                EXPECT_EQ(0, memcmp(csd_info->m.value, read_header.data(), compare_len));
            }
        };

        std::shared_ptr<EncoderConsumer> validator =
            std::make_shared<EncoderConsumer>(on_frame);

        Encode(FRAME_COUNT, true, comp, validator, { &stripe_generator } );

        EXPECT_EQ(header_update_count, 1);
    } );
}

static C2ParamValues GetConstParamValues(uint32_t four_cc)
{
    C2ParamValues const_values;

    const_values.Append(new C2ComponentDomainSetting(C2Component::DOMAIN_VIDEO));
    const_values.Append(new C2ComponentKindSetting(C2Component::KIND_ENCODER));
    const_values.Append(new C2StreamBufferTypeSetting::input(SINGLE_STREAM_ID, C2BufferData::GRAPHIC));
    const_values.Append(new C2StreamBufferTypeSetting::output(SINGLE_STREAM_ID, C2BufferData::LINEAR));
    const_values.AppendFlex(AllocUniqueString<C2PortMediaTypeSetting::input>("video/raw"));

    if (MFX_CODEC_AVC == four_cc)
        const_values.AppendFlex(AllocUniqueString<C2PortMediaTypeSetting::output>("video/avc"));

    if (MFX_CODEC_HEVC == four_cc)
        const_values.AppendFlex(AllocUniqueString<C2PortMediaTypeSetting::output>("video/hevc"));

    return const_values;
}

// Queries constant platform parameters values and checks expectations.
TEST_P(Encoder, ComponentConstParams)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [&] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        // check query through stack placeholders and the same with heap allocated
        std::vector<std::unique_ptr<C2Param>> heap_params;
        const C2ParamValues& const_values = GetConstParamValues(desc.four_cc);
        c2_blocking_t may_block{C2_MAY_BLOCK};
        c2_status_t res = comp_intf->query_vb(const_values.GetStackPointers(),
            const_values.GetIndices(), may_block, &heap_params);
        EXPECT_EQ(res, C2_OK);

        const_values.CheckStackValues();
        const_values.Check(heap_params, false);
    }); // CallComponentTest
}

INSTANTIATE_TEST_CASE_P(MfxComponents, CreateEncoder,
    ::testing::ValuesIn(g_components_desc),
    ::testing::PrintToStringParamName());

INSTANTIATE_TEST_CASE_P(MfxInvalidComponents, CreateEncoder,
    ::testing::ValuesIn(g_invalid_components_desc),
    ::testing::PrintToStringParamName());

INSTANTIATE_TEST_CASE_P(MfxComponents, Encoder,
    ::testing::ValuesIn(g_components_desc),
    ::testing::PrintToStringParamName());
