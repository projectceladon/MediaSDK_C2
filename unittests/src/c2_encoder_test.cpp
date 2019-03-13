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
#include "test_streams.h"
#include "test_params.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"
#include "mfx_c2_params.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "C2PlatformSupport.h"

#include <set>
#include <future>
#include <iostream>
#include <fstream>

using namespace android;

const float FRAME_RATE = 30.0; // 30 fps
const uint64_t FRAME_DURATION_US = (uint64_t)(1000000 / FRAME_RATE);
// Low res is chosen to speed up the tests.
const uint32_t FRAME_WIDTH = 320;
const uint32_t FRAME_HEIGHT = 240;

const uint32_t FRAME_FORMAT = HAL_PIXEL_FORMAT_NV12_TILED_INTEL; // nv12
// This frame count is required by StaticBitrate test, the encoder cannot follow
// bitrate on shorter frame sequences.
const uint32_t FRAME_COUNT = 150; // 10 default GOP size
const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

std::vector<C2ParamDescriptor> DefaultC2Params()
{
    std::vector<C2ParamDescriptor> param =
    {
        { false, "RateControl", C2RateControlSetting::PARAM_TYPE },
        { false, "FrameQP", C2FrameQPSetting::PARAM_TYPE },
        { false, "Profile", C2ProfileSetting::PARAM_TYPE },
        { false, "Level", C2LevelSetting::PARAM_TYPE },
        { false, "MemoryType", C2MemoryTypeSetting::PARAM_TYPE },
    };
    return param;
}

static std::vector<C2ParamDescriptor> h264_params_desc = DefaultC2Params();

static std::vector<C2ParamDescriptor> h265_params_desc = DefaultC2Params();

namespace {

    struct ComponentDesc
    {
        const char* component_name;
        int flags;
        c2_status_t creation_status;
        std::vector<C2ParamDescriptor> params_desc;
        C2ParamValues default_values;
        c2_status_t query_status;
        std::vector<C2ProfileLevelStruct> profile_levels;

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

C2RateControlMethod MfxRateControlToC2(mfxU16 rate_control)
{
    C2RateControlMethod res {};

    switch(rate_control) {
        case MFX_RATECONTROL_CBR:
            res = C2RateControlCBR;
            break;
        default:
            res = C2RateControlMethod(-1);
            break;
    }
    return res;
}

static C2ParamValues GetDefaultValues(const char * component_name)
{
    C2ParamValues default_values;
    // get default c2 params from mfx default structure
    mfxVideoParam video_params {};
    if (!strcmp(component_name, "C2.h264ve")) {
        video_params.mfx.CodecId = MFX_CODEC_AVC;
    } else if (!strcmp(component_name, "C2.h265ve")) {
        video_params.mfx.CodecId = MFX_CODEC_HEVC;
    } else {
        video_params.mfx.CodecId = 0; // UNKNOWN
    }

    mfx_set_defaults_mfxVideoParam_enc(&video_params);

    default_values.Append(new C2RateControlSetting(MfxRateControlToC2(video_params.mfx.RateControlMethod)));
    default_values.Append(new C2FrameRateSetting::output(0/*stream*/, C2FloatValue((float)video_params.mfx.FrameInfo.FrameRateExtN / video_params.mfx.FrameInfo.FrameRateExtD)));
    default_values.Append(new C2BitrateTuning::output(0/*stream*/, video_params.mfx.TargetKbps));
    default_values.Append(Invalidate(new C2FrameQPSetting()));
    return default_values;
}

static ComponentDesc NonExistingEncoderDesc()
{
    ComponentDesc desc {};
    desc.component_name = "C2.NonExistingEncoder";
    desc.creation_status = C2_NOT_FOUND;
    return desc;
}

static ComponentDesc g_components_desc[] = {
    { "C2.h264ve", 0, C2_OK, h264_params_desc, GetDefaultValues("C2.h264ve"), C2_CORRUPTED,
        { g_h264_profile_levels, g_h264_profile_levels + g_h264_profile_levels_count },
        &TestAvcStreamProfileLevel },
    { "C2.h265ve", 0, C2_OK, h265_params_desc, GetDefaultValues("C2.h265ve"), C2_CORRUPTED,
        { g_h265_profile_levels, g_h265_profile_levels + g_h265_profile_levels_count },
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
        sts = allocator->fetchGraphicBlock(FRAME_WIDTH, FRAME_HEIGHT, FRAME_FORMAT,
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
                uint8_t* const* data = graph_view->data();

                const C2PlanarLayout layout = graph_view->layout();
                EXPECT_NE(data, nullptr);
                for (uint32_t i = 0; i < layout.numPlanes; ++i) {
                    EXPECT_NE(data[i], nullptr);
                }

                EXPECT_EQ(FRAME_FORMAT, HAL_PIXEL_FORMAT_NV12_TILED_INTEL);

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

        C2Event event;
        event.fire(); // pre-fire as buffer is already ready to use
        C2ConstGraphicBlock const_block = block->share(block->crop(), event.fence());
        // make buffer of graphic block
        std::shared_ptr<C2Buffer> buffer = std::make_shared<C2Buffer>(MakeC2Buffer( { const_block } ));

        buffer_pack->buffers.push_back(buffer);

        std::unique_ptr<C2Worklet> worklet = std::make_unique<C2Worklet>();
        // C2 requires output items be allocated in buffers list and set to nulls
        worklet->output.buffers.push_back(nullptr);
        // work of 1 worklet
        (*work)->worklets.push_back(std::move(worklet));
    } while(false);
}

class EncoderConsumer : public C2Component::Listener
{
public:
    typedef std::function<void(const C2Worklet& worklet, const uint8_t* data, size_t length)> OnFrame;

public:
    EncoderConsumer(OnFrame on_frame, uint64_t frame_count = FRAME_COUNT)
        :on_frame_(on_frame)
        ,frame_count_(frame_count)
        ,frame_expected_(0)
    {
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

            {
                std::lock_guard<std::mutex> lock(expectations_mutex_);
                EXPECT_EQ(frame_index < frame_count_, true)
                    << "unexpected frame_index value" << frame_index;
                EXPECT_EQ(frame_index, frame_expected_)
                    << " frame " << frame_index << " is out of order";
                ++frame_expected_;
            }

            std::unique_ptr<C2ConstLinearBlock> linear_block;
            c2_status_t sts = GetC2ConstLinearBlock(buffer_pack, &linear_block);
            EXPECT_EQ(sts, C2_OK);

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
        }
        {
            std::lock_guard<std::mutex> lock(expectations_mutex_);
            // if collected all expected frames
            if(frame_expected_ >= frame_count_) {
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
    uint64_t frame_count_; // total frame count expected
    uint64_t frame_expected_; // frame index is next to come
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

    C2MemoryTypeSetting setting;
    setting.value = graphics_memory ? C2MemoryTypeGraphics : C2MemoryTypeSystem;

    std::vector<C2Param*> params = { &setting };
    std::vector<std::unique_ptr<C2SettingResult>> failures;
    std::shared_ptr<C2ComponentInterface> comp_intf = component->intf();

    c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
    EXPECT_EQ(sts, C2_OK);

    sts = component->start();
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

// Perform encoding with default parameters multiple times checking the runs give bit exact result.
// Encoding is performed on system memory in odd runs, on video memory - in even.
// If --dump-output option is set, every encoded bitstream is saved into file
// named as ./<test_case_name>/<test_name>/<component_name>-<run_index>.out,
// for example: ./MfxEncoderComponent/EncodeBitExact/C2.h264ve-0.out
// Encoded bitstream is bit exact with a result of run:
// ./mfx_transcoder64 h264 -i ./C2.h264ve.input.yuv -o ./C2-2222.h264 -nv12 -h 480 -w 640 -f 30
// -cbr -b 2222000 -CodecProfile 578 -CodecLevel 51 -TargetUsage 7 -hw
// -GopPicSize 15 -GopRefDist 1 -PicStruct 0 -NumSlice 1 -crc
TEST_P(Encoder, EncodeBitExact)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        const int TESTS_COUNT = 5;
        BinaryChunks binary[TESTS_COUNT];

        // odd runs are on graphics memory
        auto use_graphics_memory = [] (int i) -> bool { return (i % 2) != 0; };
        std::map<bool, std::string> memory_names = {
            { false, "(system memory)" },
            { true, "(video memory)" },
        };

        for(int i = 0; i < TESTS_COUNT; ++i) {

            GTestBinaryWriter writer(std::ostringstream()
                << comp_intf->getName() << "-" << i << ".out");

            StripeGenerator stripe_generator;

            EncoderConsumer::OnFrame on_frame =
                [&] (const C2Worklet&, const uint8_t* data, size_t length) {

                writer.Write(data, length);
                binary[i].PushBack(data, length);
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            Encode(FRAME_COUNT, use_graphics_memory(i), comp, validator, { &stripe_generator } );
        }
        // Every pair of results should be equal
        for (int i = 0; i < TESTS_COUNT - 1; ++i) {
            for (int j = i + 1; j < TESTS_COUNT; ++j) {
                EXPECT_EQ(binary[i], binary[j]) << "Pass " << i << memory_names[use_graphics_memory(i)]
                    << " not equal to " << j << memory_names[use_graphics_memory(j)];
            }
        }
    } );
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

// Tests if all encoding components handle config_vb with not existing parameter correctly.
// It should return individual C2SettingResult failure structure with
// initialized fields and aggregate status value.
TEST_P(Encoder, UnsupportedParam)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc&, C2CompPtr, C2CompIntfPtr comp_intf) {

        const uint32_t kParamIndexUnsupported = C2Param::TYPE_INDEX_VENDOR_START + 1000;

        typedef C2GlobalParam<C2Setting, C2Int32Value, kParamIndexUnsupported> C2UnsupportedSetting;

        C2UnsupportedSetting setting;

        std::vector<C2Param*> params = { &setting };
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        c2_blocking_t may_block{C2_MAY_BLOCK};

        c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
        EXPECT_EQ(sts, C2_BAD_INDEX);

        EXPECT_EQ(failures.size(), 1ul);

        if(failures.size() >= 1) {
            std::unique_ptr<C2SettingResult>& set_res = failures.front();

            // if a setting totally unknown for the component
            // it doesn't have info about its value of other fields
            // so return C2ParamField about whole parameter
            EXPECT_EQ(set_res->field.paramOrField, C2ParamField(&setting) );
            EXPECT_EQ(set_res->field.values, nullptr );
            EXPECT_EQ(set_res->failure, C2SettingResult::BAD_TYPE);
            EXPECT_TRUE(set_res->conflicts.empty());
        }
    } ); // CallComponentTest
}

// Synthetic input frame sequence is generated for encoder.
// It consists of striped frames where stripes figure frame index and
// white noise is applied over the frames.
// This sequence is encoded with different bitrates.
// Expected bitstream size could be calculated from bitrate set, fps, frame count.
// Actual bitstream size is checked to be no more 10% differ from expected.
TEST_P(Encoder, StaticBitrate)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        C2RateControlSetting param_rate_control;
        C2FrameRateSetting::output param_framerate;
        C2BitrateTuning::output param_bitrate;

        param_rate_control.value = C2RateControlCBR;
        param_framerate.value = FRAME_RATE;

        // these bit rates handles accurately for low res (320x240) and significant frame count (150)
        const uint32_t bitrates[] = { 100, 500, 1000 };
        const int TESTS_COUNT = MFX_GET_ARRAY_SIZE(bitrates);

        for(int test_index = 0; test_index < TESTS_COUNT; ++test_index) {

            StripeGenerator stripe_generator;
            NoiseGenerator noise_generator;

            param_bitrate.value = bitrates[test_index];

            std::vector<C2Param*> params = { &param_rate_control, &param_framerate, &param_bitrate };
            std::vector<std::unique_ptr<C2SettingResult>> failures;
            c2_blocking_t may_block{C2_MAY_BLOCK};

            c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
            EXPECT_EQ(sts, C2_OK);

            GTestBinaryWriter writer(std::ostringstream() << comp_intf->getName()
                << "-" << bitrates[test_index] << ".out");

            int64_t bitstream_len = 0;

            EncoderConsumer::OnFrame on_frame =
                [&] (const C2Worklet&, const uint8_t* data, size_t length) {

                writer.Write(data, length);
                bitstream_len += length;
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            Encode(FRAME_COUNT, false/*system memory*/, comp, validator,
                { &stripe_generator, &noise_generator } );

            int64_t expected_bitrate = 1000 * bitrates[test_index]; // target bitrate in bits
            int64_t real_bitrate = (bitstream_len * FRAME_RATE * 8) / FRAME_COUNT;
            EXPECT_TRUE(abs(real_bitrate - expected_bitrate) < expected_bitrate * 0.1)
                << "Expected bitrate: " << expected_bitrate << " Actual: " << real_bitrate
                << " for bitrate " << bitrates[test_index] << " kbit";

        }
    }); // CallComponentTest
}

// Performs encoding of the same generated YUV input
// with different rate control methods: CBR and CQP.
// Outputs should differ.
TEST_P(Encoder, StaticRateControlMethod)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        C2RateControlSetting param_rate_control;

        const C2RateControlMethod rate_control_values[] =
            { C2RateControlCBR, C2RateControlCQP };
        const int TESTS_COUNT = MFX_GET_ARRAY_SIZE(rate_control_values);
        BinaryChunks binary[TESTS_COUNT];

        for(int test_index = 0; test_index < TESTS_COUNT; ++test_index) {

            param_rate_control.value = rate_control_values[test_index];

            StripeGenerator stripe_generator;

            GTestBinaryWriter writer(std::ostringstream() <<
                comp_intf->getName() << "-" << param_rate_control.value << ".out");

            std::vector<C2Param*> params = { &param_rate_control };
            std::vector<std::unique_ptr<C2SettingResult>> failures;
            c2_blocking_t may_block{C2_MAY_BLOCK};

            c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
            EXPECT_EQ(sts, C2_OK);

            EncoderConsumer::OnFrame on_frame =
            [&] (const C2Worklet&, const uint8_t* data, size_t length) {

                writer.Write(data, length);
                binary[test_index].PushBack(data, length);
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            Encode(FRAME_COUNT, false/*system memory*/, comp, validator, { &stripe_generator } );
        }

        // Every pair of results should be equal
        for (int i = 0; i < TESTS_COUNT - 1; ++i) {
            for (int j = i + 1; j < TESTS_COUNT; ++j) {
                EXPECT_NE(binary[i], binary[j]) << "Pass " << i << " equal to " << j;
            }
        }
    }); // CallComponentTest
}

// Tests FrameQP setting (stopped state only).
// FrameQP includes qp value for I, P, B frames separately.
// The test sets them to the same value,
// if qp value is set in valid range [1..51] it expects C2_OK status and
// output bitstream smaller size when QP grows.
// If qp value is invalid, then config_vb error is expected,
// bitstream must be bit exact with previous run.
TEST_P(Encoder, StaticFrameQP)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        C2RateControlSetting param_rate_control;
        param_rate_control.value = C2RateControlCQP;

        C2FrameQPSetting param_qp;

        // set rate control method to CQP separately
        // if set together with QP value -> QP is reset to default value (30)
        // and test runs where qp is set to invalid values don't work
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        std::vector<C2Param*> params = { &param_rate_control };
        c2_blocking_t may_block{C2_MAY_BLOCK};
        c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_EQ(failures.size(), 0ul);

        struct TestRun {
            uint32_t qp;
            c2_status_t expected_result;
        };

        const TestRun test_runs[] = {
            { 25, C2_OK },
            { 0, C2_BAD_VALUE },
            { 30, C2_OK },
            { 35, C2_OK },
            { 100, C2_BAD_VALUE },
        };

        const int TESTS_COUNT = MFX_GET_ARRAY_SIZE(test_runs);

        // at least 2 successful runs to compare smaller/greater output bitstream
        ASSERT_GE(std::count_if(test_runs, test_runs + TESTS_COUNT,
            [] (const TestRun& run) { return run.expected_result == C2_OK; } ), 2);
        ASSERT_EQ(test_runs[0].expected_result, C2_OK); // first encode must be ok to compare with

        uint32_t prev_bitstream_len = 0, prev_valid_qp = 0;
        BinaryChunks prev_bitstream;

        for(const TestRun& test_run : test_runs) {

            StripeGenerator stripe_generator;
            NoiseGenerator noise_generator;
            BinaryChunks bitstream;
            uint32_t bitstream_len = 0;

            param_qp.qp_i = test_run.qp;
            param_qp.qp_p = test_run.qp;
            param_qp.qp_b = test_run.qp;

            std::vector<C2Param*> params = { &param_qp };
            c2_blocking_t may_block{C2_MAY_BLOCK};
            c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
            EXPECT_EQ(sts, test_run.expected_result);
            if(test_run.expected_result == C2_OK) {
                EXPECT_EQ(failures.size(), 0ul);
            } else {
                EXPECT_EQ(failures.size(), 3ul);
                EXPECT_TRUE(failures.size() > 0 && failures[0]->field.paramOrField == C2ParamField(&param_qp, &C2FrameQPSetting::qp_i));
                EXPECT_TRUE(failures.size() > 1 && failures[1]->field.paramOrField == C2ParamField(&param_qp, &C2FrameQPSetting::qp_p));
                EXPECT_TRUE(failures.size() > 2 && failures[2]->field.paramOrField == C2ParamField(&param_qp, &C2FrameQPSetting::qp_b));

                for(const std::unique_ptr<C2SettingResult>& set_res : failures) {
                    EXPECT_EQ(set_res->failure, C2SettingResult::BAD_VALUE);
                    EXPECT_NE(set_res->field.values, nullptr);
                    if(nullptr != set_res->field.values) {
                        EXPECT_EQ(set_res->field.values->type, C2FieldSupportedValues::RANGE);
                        EXPECT_EQ(set_res->field.values->range.min.u32, 1ul);
                        EXPECT_EQ(set_res->field.values->range.max.u32, 51u);
                        EXPECT_EQ(set_res->field.values->range.step.u32, 1u);
                        EXPECT_EQ(set_res->field.values->range.num.u32, 1u);
                        EXPECT_EQ(set_res->field.values->range.denom.u32, 1u);
                    }
                    EXPECT_TRUE(set_res->conflicts.empty());
                }
            }

            EncoderConsumer::OnFrame on_frame =
                [&] (const C2Worklet&, const uint8_t* data, size_t length) {

                bitstream.PushBack(data, length);
                bitstream_len += length;
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            // validator checks that encoder correctly behaves on the changed config
            Encode(FRAME_COUNT, false/*system memory*/, comp, validator, { &stripe_generator, &noise_generator } );

            if(&test_run != &test_runs[0]) { // nothing to compare on first run
                if(test_run.expected_result == C2_OK) {
                    EXPECT_TRUE(test_run.qp > prev_valid_qp);
                    EXPECT_TRUE(bitstream_len < prev_bitstream_len)
                        << "Outputs size " << prev_bitstream_len << " is not bigger "
                        << "outputs size " << bitstream_len;
                } else {
                    EXPECT_EQ(bitstream, prev_bitstream) << "bitstream should not change when params config failed.";
                }
            }

            if(test_run.expected_result == C2_OK) {
                prev_bitstream_len = bitstream_len;
                prev_bitstream = bitstream;
                prev_valid_qp = test_run.qp;
            }
        }
    }); // CallComponentTest
}

// Queries param values and verify correct defaults.
// Does check before encoding (STOPPED state), during encoding on every frame,
// and after encoding.
TEST_P(Encoder, query_vb)
{
    ComponentsCache::GetInstance()->Clear(); // reset cache to re-create components and have default params there

    CallComponentTest<ComponentDesc>(GetParam(),
        [&] (const ComponentDesc& comp_desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        (void)comp;

        auto check_default_values = [&] () {
            // check query through stack placeholders and the same with heap allocated
            std::vector<std::unique_ptr<C2Param>> heap_params;
            const C2ParamValues& default_values = comp_desc.default_values;
            c2_blocking_t may_block{C2_MAY_BLOCK};
            c2_status_t res = comp_intf->query_vb(default_values.GetStackPointers(),
                default_values.GetIndices(), may_block, &heap_params);
            EXPECT_EQ(res, comp_desc.query_status);

            default_values.CheckStackValues();
            default_values.Check(heap_params, true);
        };

        {
            SCOPED_TRACE("Before encode");
            check_default_values();
        }

        StripeGenerator stripe_generator;

        EncoderConsumer::OnFrame on_frame =
            [&] (const C2Worklet&, const uint8_t*, size_t) {

            SCOPED_TRACE("During encode");
            check_default_values();
        };

        std::shared_ptr<EncoderConsumer> validator =
            std::make_shared<EncoderConsumer>(on_frame);

        Encode(FRAME_COUNT, false/*system memory*/, comp, validator, { &stripe_generator } );

        {
            SCOPED_TRACE("After encode");
            check_default_values();
        }
    }); // CallComponentTest
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
            if (!strcmp(component_name, "C2.h264ve")) {
                nal_unit_type = (uint8_t)header_byte & 0x1F;
                const uint8_t IDR_SLICE = 5;
                if (nal_unit_type == IDR_SLICE) {
                    ++count;
                }
            } else if (!strcmp(component_name, "C2.h265ve")) {
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

// Tests dynamic parameter enforcing IDR frame to be inserted into encoded bitstream.
// Encodes the same frames multiple times, inserting IDR every N frames.
// Checks that output bitstream contains idr frames exactly as expected.
// It tries to request IDR frame with config_vb and with C2Work structure.
TEST_P(Encoder, IntraRefresh)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [&] (const ComponentDesc& comp_desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {
        (void)comp_desc;
        (void)comp;

        for(bool use_config_nb : { true, false }) {

            SCOPED_TRACE((use_config_nb ? "config_vb" : "C2Work"));

            std::vector<int> idr_distances { 2, 3, 7, 10, 15 };

            for (int idr_distance : idr_distances) {

                StripeGenerator stripe_generator;
                NoiseGenerator noise_generator;
                std::vector<char> bitstream;

                GTestBinaryWriter writer(std::ostringstream()
                    << comp_intf->getName() << "-" << idr_distance << ".out");

                BeforeQueueWork before_queue_work = [&] (uint32_t frame_index, C2Work* work) {

                    if ((frame_index % idr_distance) == 0) {

                        std::unique_ptr<C2IntraRefreshTuning> intra_refresh =
                            std::make_unique<C2IntraRefreshTuning>();
                        intra_refresh->value = true;
                        if (use_config_nb) {
                            std::vector<C2Param*> params { intra_refresh.get() };
                            std::vector<std::unique_ptr<C2SettingResult>> failures;
                            c2_blocking_t may_block{C2_MAY_BLOCK};
                            c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);

                            EXPECT_EQ(sts, C2_OK);
                            EXPECT_EQ(failures.size(), 0ul);
                        } else {
                            ASSERT_EQ(work->worklets.size(), 1ul);
                            C2Worklet* worklet = work->worklets.front().get();
                            ASSERT_NE(worklet, nullptr);
                            worklet->tunings.push_back(std::move(intra_refresh));
                        }
                    }
                };

                EncoderConsumer::OnFrame on_frame =
                    [&] (const C2Worklet&, const uint8_t* data, size_t length) {

                    const char* ch_data = (const char*)data;
                    std::copy(ch_data, ch_data + length, std::back_inserter(bitstream));
                    writer.Write(data, length);
                };

                std::shared_ptr<EncoderConsumer> validator =
                    std::make_shared<EncoderConsumer>(on_frame);

                Encode(FRAME_COUNT, false/*system memory*/, comp, validator, { &stripe_generator, &noise_generator },
                    before_queue_work );

                uint32_t idr_expected = (FRAME_COUNT - 1) / idr_distance + 1;

                uint32_t idr_actual = CountIdrSlices(std::move(bitstream), comp_desc.component_name);

                EXPECT_EQ(idr_expected, idr_actual) << NAMED(idr_expected) << NAMED(idr_actual)
                    << NAMED(idr_distance);
            }
        }
    }); // CallComponentTest
}

// First half of video is encoded with one bitrate, second with another.
// Checks that output bitrate is changed accordingly.
// Bitrate is changed with config_vb and with C2Work structure on separate passes.
// The bitrate tuning is done in VBR mode, as it is the only mode media SDK supports
// dynamic bitrate change.
TEST_P(Encoder, DynamicBitrate)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [&] (const ComponentDesc& comp_desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {
        (void)comp_desc;
        (void)comp;

        C2RateControlSetting param_rate_control;
        C2FrameRateSetting::output param_framerate;

        param_rate_control.value = C2RateControlVBR;
        param_framerate.value = FRAME_RATE;

        std::vector<C2Param*> static_params { &param_rate_control, &param_framerate };
        std::vector<std::unique_ptr<C2SettingResult>> failures;

        c2_blocking_t may_block{C2_MAY_BLOCK};
        c2_status_t sts = comp_intf->config_vb(static_params, may_block, &failures);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_EQ(failures.size(), 0ul);

        const uint32_t TEST_FRAME_COUNT = FRAME_COUNT * 2;

        for(bool use_config_nb : { true, false }) {

            SCOPED_TRACE((use_config_nb ? "config_vb" : "C2Work"));

            std::unique_ptr<C2BitrateTuning::output> param_bitrate =
                std::make_unique<C2BitrateTuning::output>();

            const uint32_t BITRATE_1 = 100;
            const uint32_t MULTIPLIER = 2;
            const uint32_t BITRATE_2 = BITRATE_1 * MULTIPLIER;

            size_t stream_len_1 = 0;
            size_t stream_len_2 = 0;

            StripeGenerator stripe_generator;
            NoiseGenerator noise_generator;

            GTestBinaryWriter writer(std::ostringstream()
                << comp_intf->getName() << "-" << (int)use_config_nb << ".out");

            param_bitrate->value = BITRATE_1;

            std::vector<C2Param*> dynamic_params = { param_bitrate.get() };
            c2_blocking_t may_block{C2_MAY_BLOCK};

            c2_status_t sts = comp_intf->config_vb(dynamic_params, may_block, &failures);
            EXPECT_EQ(sts, C2_OK);
            EXPECT_EQ(failures.size(), 0ul);

            BeforeQueueWork before_queue_work = [&] (uint32_t frame_index, C2Work* work) {

                if (frame_index == TEST_FRAME_COUNT / 2) {

                    param_bitrate->value = BITRATE_2;

                    if (use_config_nb) {
                        c2_status_t sts = comp_intf->config_vb(dynamic_params, may_block, &failures);

                        EXPECT_EQ(sts, C2_OK);
                        EXPECT_EQ(failures.size(), 0ul);
                    } else {
                        ASSERT_EQ(work->worklets.size(), 1ul);
                        C2Worklet* worklet = work->worklets.front().get();
                        ASSERT_NE(worklet, nullptr);
                        worklet->tunings.push_back(std::move(param_bitrate));
                    }
                }
            };

            EncoderConsumer::OnFrame on_frame =
                [&] (const C2Worklet& worklet, const uint8_t* data, size_t length) {

                uint64_t frame_index = worklet.output.ordinal.frameIndex.peeku();
                if (frame_index < TEST_FRAME_COUNT / 2) {
                    stream_len_1 += length;
                } else {
                    stream_len_2 += length;
                }

                writer.Write(data, length);
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame, TEST_FRAME_COUNT);

            Encode(TEST_FRAME_COUNT, false/*system memory*/, comp, validator, { &stripe_generator, &noise_generator },
                before_queue_work );

            int64_t real_bitrate_1 = (stream_len_1 * FRAME_RATE * 8) / FRAME_COUNT;
            int64_t real_bitrate_2 = (stream_len_2 * FRAME_RATE * 8) / FRAME_COUNT;

            EXPECT_TRUE(abs(real_bitrate_1 - BITRATE_1 * 1000) < BITRATE_1 * 1000 * 0.1)
                << "Expected bitrate: " << BITRATE_1 * 1000 << " Actual: " << real_bitrate_1;

            EXPECT_TRUE(abs(real_bitrate_2 - BITRATE_2 * 1000) < BITRATE_2 * 1000 * 0.1)
                << "Expected bitrate: " << BITRATE_2 * 1000 << " Actual: " << real_bitrate_2;
        }
    }); // CallComponentTest
}

// Queries array of supported pairs (profile, level) and compares to expected array.
TEST_P(Encoder, ProfileLevelInfo)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [&] (const ComponentDesc& comp_desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {
        (void)comp;

        std::vector<std::unique_ptr<C2Param>> heap_params;
        c2_blocking_t may_block{C2_MAY_BLOCK};
        c2_status_t res = comp_intf->query_vb(
            {} , { C2ProfileLevelInfo::output::PARAM_TYPE }, may_block, &heap_params);
        EXPECT_EQ(res, C2_OK);
        EXPECT_EQ(heap_params.size(), 1ul);

        if (heap_params.size() > 0) {
            C2Param* param = heap_params[0].get();

            EXPECT_TRUE(param->isFlexible());
            EXPECT_EQ(param->type(), C2ProfileLevelInfo::output::PARAM_TYPE);

            if (param->type() == C2ProfileLevelInfo::output::PARAM_TYPE) {
                C2ProfileLevelInfo* info = (C2ProfileLevelInfo*)param;
                EXPECT_EQ(info->flexCount(), comp_desc.profile_levels.size());

                size_t to_compare = std::min(info->flexCount(), comp_desc.profile_levels.size());
                for (size_t i = 0; i < to_compare; ++i) {
                    EXPECT_EQ(info->m.values[i].profile, comp_desc.profile_levels[i].profile);
                    EXPECT_EQ(info->m.values[i].level, comp_desc.profile_levels[i].level);
                }
            }
        }
    }); // CallComponentTest
}

// Specifies various values for profile and level,
// checks they are queried back fine.
// Encodes stream and checks sps of encoded bitstreams fit passed profile and level.
TEST_P(Encoder, CodecProfileAndLevel)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& comp_desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        for(const C2ProfileLevelStruct& test_run : comp_desc.profile_levels) {

            StripeGenerator stripe_generator;
            NoiseGenerator noise_generator;
            std::vector<char> bitstream;

            #define TEST_RUN_NAME std::hex << "0x" << test_run.profile << "-0x" << test_run.level

            SCOPED_TRACE(testing::Message() << TEST_RUN_NAME);

            GTestBinaryWriter writer(std::ostringstream()
                << comp_intf->getName() << "-" << TEST_RUN_NAME << ".out");

            C2ProfileSetting param_profile(test_run.profile);
            C2LevelSetting param_level(test_run.level);
            std::vector<C2Param*> params = { &param_profile, &param_level };
            std::vector<std::unique_ptr<C2SettingResult>> failures;

            c2_blocking_t may_block{C2_MAY_BLOCK};
            c2_status_t sts = comp_intf->config_vb(params, may_block, &failures);
            EXPECT_EQ(sts, C2_OK);
            EXPECT_EQ(failures.size(), 0ul);

            C2ParamValues query_expected;
            query_expected.Append(new C2ProfileSetting(test_run.profile));
            query_expected.Append(new C2LevelSetting(test_run.level));
            sts = comp_intf->query_vb(query_expected.GetStackPointers(),
                {}, may_block, nullptr);
            EXPECT_EQ(sts, C2_OK);
            query_expected.CheckStackValues();

            EncoderConsumer::OnFrame on_frame =
                [&] (const C2Worklet&, const uint8_t* data, size_t length) {

                const char* ch_data = (const char*)data;
                std::copy(ch_data, ch_data + length, std::back_inserter(bitstream));
                writer.Write(data, length);
            };

            const int TEST_FRAME_COUNT = 1;
            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame, TEST_FRAME_COUNT);

            Encode(TEST_FRAME_COUNT, false/*system memory*/, comp, validator,
                { &stripe_generator, &noise_generator } );

            std::string error_message;
            bool stream_ok = comp_desc.test_stream_profile_level(test_run, std::move(bitstream), &error_message);
            EXPECT_TRUE(stream_ok) << error_message;
        }
    }); // CallComponentTest
}

bool CheckFrameRateInStream(std::vector<char>&& contents, const float expected,  const char* component_name, std::string* message)
{
    std::ostringstream oss;
    bool res = false;
    if (!strcmp(component_name, "C2.h264ve")) {
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
    } else if (!strcmp(component_name, "C2.h265ve")) {
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

// Specifies various values for frame rate,
// checks they are queried back fine,
// check real FrameRate using size of encoded stream
TEST_P(Encoder, FrameRate)
{
    CallComponentTest<ComponentDesc>(GetParam(),
        [] (const ComponentDesc& comp_desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        std::vector<char> bitstream;
        struct TestRunDescription
        {
            float expected_framerate;
            size_t stream_len;
        };
        TestRunDescription test_runs[2] = { { 25.0, 0 }, { 50.0, 0 } };
        const uint32_t CONST_BITRATE = 300;

        C2BitrateTuning::output param_bitrate;
        C2RateControlSetting param_rate_control;

        param_bitrate.value = CONST_BITRATE;
        param_rate_control.value = C2RateControlCBR;

        std::vector<std::unique_ptr<C2SettingResult>> failures;
        std::vector<C2Param*> static_params = { &param_rate_control, &param_bitrate };

        c2_blocking_t may_block{C2_MAY_BLOCK};

        c2_status_t sts = comp_intf->config_vb(static_params, may_block, &failures);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_EQ(failures.size(), 0ul);

        for (auto test_run : test_runs) {

            StripeGenerator stripe_generator;
            NoiseGenerator noise_generator;

            C2FrameRateSetting::output param_framerate;
            param_framerate.value = test_run.expected_framerate;
            std::vector<C2Param*> dynamic_params = { &param_framerate };

            GTestBinaryWriter writer(std::ostringstream()
                << comp_intf->getName() << "-" << test_run.expected_framerate << ".out");

            sts = comp_intf->config_vb(dynamic_params, may_block, &failures);
            EXPECT_EQ(sts, C2_OK);
            EXPECT_EQ(failures.size(), 0ul);

            C2ParamValues query_expected;
            query_expected.Append(new C2FrameRateSetting::output(0/*stream*/, C2FloatValue(test_run.expected_framerate)));

            sts = comp_intf->query_vb(query_expected.GetStackPointers(),
                {}, may_block, nullptr);
            EXPECT_EQ(sts, C2_OK);
            query_expected.CheckStackValues();

            EncoderConsumer::OnFrame on_frame =
                [&] (const C2Worklet&, const uint8_t* data, size_t length) {

                const char* ch_data = (const char*)data;
                std::copy(ch_data, ch_data + length, std::back_inserter(bitstream));
                test_run.stream_len += length;
                writer.Write(data, length);
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame, FRAME_COUNT);

            Encode(FRAME_COUNT, false/*system memory*/, comp, validator,
                { &stripe_generator, &noise_generator } );

            std::string error_message;
            bool stream_ok = CheckFrameRateInStream(std::move(bitstream), test_run.expected_framerate, comp_desc.component_name, &error_message);
            EXPECT_TRUE(stream_ok) << error_message;

            float real_framerate = (CONST_BITRATE * FRAME_COUNT * 1000.0) /
                (test_run.stream_len * 8);
            EXPECT_TRUE(abs(real_framerate - test_run.expected_framerate) < test_run.expected_framerate * 0.2)
                << "Expected framerate: " << test_run.expected_framerate << " Actual: " << real_framerate;
        }

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
