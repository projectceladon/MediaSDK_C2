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
#include "test_params.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"
#include "mfx_c2_params.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "C2BlockAllocator.h"

#include <set>
#include <future>
#include <iostream>
#include <fstream>

using namespace android;

const uint64_t FRAME_DURATION_US = 33333; // 30 fps
// Low res is chosen to speed up the tests.
const uint32_t FRAME_WIDTH = 320;
const uint32_t FRAME_HEIGHT = 240;
const uint32_t FRAME_BUF_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3 / 2;

const uint32_t FRAME_FORMAT = 0; // nv12
// This frame count is required by StaticBitrate test, the encoder cannot follow
// bitrate on shorter frame sequences.
const uint32_t FRAME_COUNT = 150; // 10 default GOP size
const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

static std::vector<C2ParamDescriptor> h264_params_desc =
{
    { false, "RateControl", C2RateControlSetting::typeIndex },
    { false, "Bitrate", C2BitrateTuning::typeIndex },
    { false, "FrameQP", C2FrameQPSetting::typeIndex },
};

struct ComponentDesc
{
    const char* component_name;
    int flags;
    status_t creation_status;
    std::vector<C2ParamDescriptor> params_desc;
    C2ParamValues default_values;
    status_t query_status;
};

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

static C2ParamValues GetH264DefaultValues()
{
    C2ParamValues default_values;
    // get default c2 params from mfx default structure
    mfxVideoParam video_params {};
    video_params.mfx.CodecId = MFX_CODEC_AVC;
    mfx_set_defaults_mfxVideoParam_enc(&video_params);

    default_values.Append(new C2RateControlSetting(MfxRateControlToC2(video_params.mfx.RateControlMethod)));
    default_values.Append(new C2BitrateTuning(video_params.mfx.TargetKbps));
    default_values.Append(Invalidate(new C2FrameQPSetting()));
    return default_values;
}

static ComponentDesc g_components_desc[] = {
    { "C2.h264ve", 0, C2_OK, h264_params_desc, GetH264DefaultValues(), C2_CORRUPTED },
    { "C2.NonExistingEncoder", 0, C2_NOT_FOUND, {}, {}, {} },
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
    auto components_cache = GetComponentsCache();

    auto it = components_cache.find(name);
    if(it != components_cache.end()) {
        result = it->second;
    }
    else {
        const ComponentDesc* desc = GetComponentDesc(name);
        ASSERT_NE(desc, nullptr);

        MfxC2Component* mfx_component;
        status_t status = MfxCreateC2Component(name, desc->flags, &mfx_component);
        EXPECT_EQ(status, desc->creation_status);
        if(desc->creation_status == C2_OK) {
            EXPECT_NE(mfx_component, nullptr);
            result = std::shared_ptr<MfxC2Component>(mfx_component);

            components_cache.emplace(name, result);
        }
    }
    return result;
}

// Assures that all encoding components might be successfully created.
// NonExistingEncoder cannot be created and C2_NOT_FOUND error is returned.
TEST(MfxEncoderComponent, Create)
{
    for(const auto& desc : g_components_desc) {

        std::shared_ptr<MfxC2Component> encoder = GetCachedComponent(desc.component_name);

        EXPECT_EQ(encoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;
    }
}

// Checks that all successfully created encoding components expose C2ComponentInterface
// and return correct information once queried (component name).
TEST(MfxEncoderComponent, intf)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        EXPECT_EQ(comp_intf->getName(), desc.component_name);
    } );
}

static void PrepareWork(uint32_t frame_index,
    std::unique_ptr<C2Work>* work,
    const std::vector<FrameGenerator*>& generators)
{
    *work = std::make_unique<C2Work>();
    C2BufferPack* buffer_pack = &((*work)->input);

    if(frame_index < FRAME_COUNT - 1) {
        buffer_pack->flags = flags_t(0);
    } else {
        buffer_pack->flags = BUFFERFLAG_END_OF_STREAM;
    }

    // Set up frame header properties:
    // timestamp is set to correspond to 30 fps stream.
    buffer_pack->ordinal.timestamp = FRAME_DURATION_US * frame_index;
    buffer_pack->ordinal.frame_index = frame_index;
    buffer_pack->ordinal.custom_ordinal = 0;

    do {

        std::shared_ptr<android::C2BlockAllocator> allocator;
        android::status_t sts = GetC2BlockAllocator(&allocator);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(allocator, nullptr);

        if(nullptr == allocator) break;

        C2MemoryUsage mem_usage = { C2MemoryUsage::kSoftwareRead, C2MemoryUsage::kSoftwareWrite };
        std::shared_ptr<C2GraphicBlock> block;
        sts = allocator->allocateGraphicBlock(FRAME_WIDTH, FRAME_HEIGHT, FRAME_FORMAT,
            mem_usage, &block);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(block, nullptr);

        if(nullptr == block) break;

        uint8_t* data = nullptr;
        sts = MapGraphicBlock(*block, TIMEOUT_NS, &data);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(data, nullptr);

        const uint32_t stride = FRAME_WIDTH;

        for(FrameGenerator* generator : generators) {
            generator->Apply(frame_index, data, FRAME_WIDTH, stride, FRAME_HEIGHT);
        }

        C2Event event;
        event.fire(); // pre-fire as buffer is already ready to use
        C2ConstGraphicBlock const_block = block->share(block->crop(), event.fence());
        // make buffer of graphic block
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

class EncoderConsumer : public C2ComponentListener
{
public:
    typedef std::function<void(const uint8_t* data, size_t length)> OnFrame;

public:
    EncoderConsumer(OnFrame on_frame)
        :on_frame_(on_frame)
        ,frame_expected_(0)
    {
    }

    // future ready when validator got all expected frames
    std::future<void> GetFuture()
    {
        return done_.get_future();
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

            EXPECT_EQ(buffer_pack.ordinal.timestamp, frame_index * FRAME_DURATION_US); // 30 fps

            EXPECT_EQ(frame_index < FRAME_COUNT, true)
                << "unexpected frame_index value" << frame_index;
            EXPECT_EQ(frame_index, frame_expected_)
                << " frame " << frame_index << " is out of order";

            ++frame_expected_;

            std::unique_ptr<C2ConstLinearBlock> linear_block;
            C2Error sts = GetC2ConstLinearBlock(buffer_pack, &linear_block);
            EXPECT_EQ(sts, C2_OK);

            if(nullptr != linear_block) {

                const uint8_t* raw  = nullptr;

                sts = MapConstLinearBlock(*linear_block, TIMEOUT_NS, &raw);
                EXPECT_EQ(sts, C2_OK);
                EXPECT_NE(raw, nullptr);
                EXPECT_NE(linear_block->size(), 0);

                if(nullptr != raw) {
                    on_frame_(raw + linear_block->offset(), linear_block->size());
                }
            }
        }
        // if collected all expected frames
        if(frame_expected_ >= FRAME_COUNT) {
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
    uint64_t frame_expected_; // frame index is next to come
    std::promise<void> done_; // fire when all expected frames came
};

static void Encode(
    uint32_t frame_count,
    std::shared_ptr<C2Component> component,
    std::shared_ptr<EncoderConsumer> validator,
    const std::vector<FrameGenerator*>& generators)
{
    component->registerListener(validator);

    status_t sts = component->start();
    EXPECT_EQ(sts, C2_OK);

    NoiseGenerator noise_generator;

    for(uint32_t frame_index = 0; frame_index < frame_count; ++frame_index) {
        // prepare worklet and push
        std::unique_ptr<C2Work> work;

        // insert input data
        PrepareWork(frame_index, &work, generators);
        std::list<std::unique_ptr<C2Work>> works;
        works.push_back(std::move(work));

        sts = component->queue_nb(&works);
        EXPECT_EQ(sts, C2_OK);
    }

    std::future<void> future = validator->GetFuture();
    std::future_status future_sts = future.wait_for(std::chrono::seconds(10));
    EXPECT_EQ(future_sts, std::future_status::ready) << " encoded less frames than expected";

    component->unregisterListener(validator);
    sts = component->stop();
    EXPECT_EQ(sts, C2_OK);
}

// Perform encoding with default parameters multiple times checking the runs give bit exact result.
// If --dump-output option is set, every encoded bitstream is saved into file
// named as ./<test_case_name>/<test_name>/<component_name>-<run_index>.out,
// for example: ./MfxEncoderComponent/EncodeBitExact/C2.h264ve-0.out
// Encoded bitstream is bit exact with a result of run:
// ./mfx_transcoder64 h264 -i ./C2.h264ve.input.yuv -o ./C2-2222.h264 -nv12 -h 480 -w 640 -f 30
// -cbr -b 2222000 -CodecProfile 578 -CodecLevel 51 -TargetUsage 7 -hw
// -GopPicSize 15 -GopRefDist 1 -PicStruct 0 -NumSlice 1 -crc

TEST(MfxEncoderComponent, EncodeBitExact)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        const int TESTS_COUNT = 5;
        BinaryChunks binary[TESTS_COUNT];

        for(int i = 0; i < TESTS_COUNT; ++i) {

            GTestBinaryWriter writer(std::ostringstream()
                << comp_intf->getName() << "-" << i << ".out");

            StripeGenerator stripe_generator;

            EncoderConsumer::OnFrame on_frame = [&] (const uint8_t* data, size_t length) {
                writer.Write(data, length);
                binary[i].PushBack(data, length);
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            Encode(FRAME_COUNT, comp, validator, { &stripe_generator } );
        }
        // Every pair of results should be equal
        for (int i = 0; i < TESTS_COUNT - 1; ++i) {
            for (int j = i + 1; j < TESTS_COUNT; ++j) {
                EXPECT_EQ(binary[i], binary[j]) << "Pass " << i << " not equal to " << j;
            }
        }
    } );
}

// Checks the correctness of all encoding components state machine.
// The component should be able to start from STOPPED (initial) state,
// stop from RUNNING state. Otherwise, C2_BAD_STATE should be returned.
TEST(MfxEncoderComponent, State)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr) {

        status_t sts = C2_OK;

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
TEST(MfxEncoderComponent, getSupportedParams)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        std::vector<std::shared_ptr<C2ParamDescriptor>> params_actual;
        status_t sts = comp_intf->getSupportedParams(&params_actual);
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

// Tests if all encoding components handle config_nb with not existing parameter correctly.
// It should return individual C2SettingResult failure structure with
// initialized fields and aggregate status value.
TEST(MfxEncoderComponent, UnsupportedParam)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr, C2CompIntfPtr comp_intf) {

        const uint32_t kParamIndexUnsupported = C2Param::BaseIndex::kVendorStart + 1000;

        typedef C2GlobalParam<C2Setting, C2Int32Value, kParamIndexUnsupported> C2UnsupportedSetting;

        C2UnsupportedSetting setting;

        std::vector<C2Param* const> params = { &setting };
        std::vector<std::unique_ptr<C2SettingResult>> failures;

        status_t sts = comp_intf->config_nb(params, &failures);
        EXPECT_EQ(sts, C2_BAD_INDEX);

        EXPECT_EQ(failures.size(), 1);

        if(failures.size() >= 1) {
            std::unique_ptr<C2SettingResult>& set_res = failures.front();

            // if a setting totally unknown for the component
            // it doesn't have info about its mValue of other fields
            // so return C2ParamField about whole parameter
            EXPECT_EQ(set_res->field, C2ParamField(&setting));
            EXPECT_EQ(set_res->failure, C2SettingResult::BAD_TYPE);
            EXPECT_EQ(set_res->supportedValues, nullptr);
            EXPECT_TRUE(set_res->conflictingFields.empty());
        }
    } ); // ForEveryComponent
}

// Synthetic input frame sequence is generated for encoder.
// It consists of striped frames where stripes figure frame index and
// white noise is applied over the frames.
// This sequence is encoded with different bitrates.
// Expected bitstream size could be calculated from bitrate set, fps, frame count.
// Actual bitstream size is checked to be no more 10% differ from expected.
TEST(MfxEncoderComponent, StaticBitrate)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        C2RateControlSetting param_rate_control;
        param_rate_control.mValue = C2RateControlCBR;
        C2BitrateTuning param_bitrate;

        // these bit rates handles accurately for low res (320x240) and significant frame count (150)
        const uint32_t bitrates[] = { 100, 500, 1000 };
        const int TESTS_COUNT = MFX_GET_ARRAY_SIZE(bitrates);

        for(int test_index = 0; test_index < TESTS_COUNT; ++test_index) {

            StripeGenerator stripe_generator;
            NoiseGenerator noise_generator;

            param_bitrate.mValue = bitrates[test_index];

            std::vector<C2Param* const> params = { &param_rate_control, &param_bitrate };
            std::vector<std::unique_ptr<C2SettingResult>> failures;

            status_t sts = comp_intf->config_nb(params, &failures);
            EXPECT_EQ(sts, C2_OK);

            GTestBinaryWriter writer(std::ostringstream() << comp_intf->getName()
                << "-" << bitrates[test_index] << ".out");

            int64_t bitstream_len = 0;

            EncoderConsumer::OnFrame on_frame =
                [&] (const uint8_t* data, size_t length) {

                writer.Write(data, length);
                bitstream_len += length;
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            Encode(FRAME_COUNT, comp, validator, { &stripe_generator, &noise_generator } );

            int64_t expected_len = FRAME_DURATION_US * FRAME_COUNT * 1000 * bitrates[test_index] /
                (8 * 1000000);
            EXPECT_TRUE(abs(bitstream_len - expected_len) < expected_len * 0.1)
                << "Expected bitstream len: " << expected_len << " Actual: " << bitstream_len
                << " for bitrate " << bitrates[test_index] << " kbit";

        }
    }); // ForEveryComponent
}

// Performs encoding of the same generated YUV input
// with different rate control methods: CBR and CQP.
// Outputs should differ.
TEST(MfxEncoderComponent, StaticRateControlMethod)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        C2RateControlSetting param_rate_control;

        const C2RateControlMethod rate_control_values[] =
            { C2RateControlCBR, C2RateControlCQP };
        const int TESTS_COUNT = MFX_GET_ARRAY_SIZE(rate_control_values);
        BinaryChunks binary[TESTS_COUNT];

        for(int test_index = 0; test_index < TESTS_COUNT; ++test_index) {

            param_rate_control.mValue = rate_control_values[test_index];

            StripeGenerator stripe_generator;

            GTestBinaryWriter writer(std::ostringstream() <<
                comp_intf->getName() << "-" << param_rate_control.mValue << ".out");

            std::vector<C2Param* const> params = { &param_rate_control };
            std::vector<std::unique_ptr<C2SettingResult>> failures;

            status_t sts = comp_intf->config_nb(params, &failures);
            EXPECT_EQ(sts, C2_OK);

            EncoderConsumer::OnFrame on_frame = [&] (const uint8_t* data, size_t length) {
                writer.Write(data, length);
                binary[test_index].PushBack(data, length);
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            Encode(FRAME_COUNT, comp, validator, { &stripe_generator } );
        }

        // Every pair of results should be equal
        for (int i = 0; i < TESTS_COUNT - 1; ++i) {
            for (int j = i + 1; j < TESTS_COUNT; ++j) {
                EXPECT_NE(binary[i], binary[j]) << "Pass " << i << " equal to " << j;
            }
        }
    }); // ForEveryComponent
}

// Tests FrameQP setting (stopped state only).
// FrameQP includes qp value for I, P, B frames separately.
// The test sets them to the same value,
// if qp value is set in valid range [1..51] it expects C2_OK status and
// output bitstream smaller size when QP grows.
// If qp value is invalid, then config_nb error is expected,
// bitstream must be bit exact with previous run.
TEST(MfxEncoderComponent, StaticFrameQP)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        C2RateControlSetting param_rate_control;
        param_rate_control.mValue = C2RateControlCQP;

        C2FrameQPSetting param_qp;

        // set rate control method to CQP separately
        // if set together with QP value -> QP is reset to default value (30)
        // and test runs where qp is set to invalid values don't work
        std::vector<std::unique_ptr<C2SettingResult>> failures;
        std::vector<C2Param* const> params = { &param_rate_control };
        status_t sts = comp_intf->config_nb(params, &failures);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_EQ(failures.size(), 0);

        struct TestRun {
            uint32_t qp;
            status_t expected_result;
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

            std::vector<C2Param* const> params = { &param_qp };
            status_t sts = comp_intf->config_nb(params, &failures);
            EXPECT_EQ(sts, test_run.expected_result);
            if(test_run.expected_result == C2_OK) {
                EXPECT_EQ(failures.size(), 0);
            } else {
                EXPECT_EQ(failures.size(), 3);
                EXPECT_TRUE(failures.size() > 0 && failures[0]->field == C2ParamField(&param_qp, &C2FrameQPSetting::qp_i));
                EXPECT_TRUE(failures.size() > 1 && failures[1]->field == C2ParamField(&param_qp, &C2FrameQPSetting::qp_p));
                EXPECT_TRUE(failures.size() > 2 && failures[2]->field == C2ParamField(&param_qp, &C2FrameQPSetting::qp_b));

                for(const std::unique_ptr<C2SettingResult>& set_res : failures) {
                    EXPECT_EQ(set_res->failure, C2SettingResult::BAD_VALUE);
                    EXPECT_NE(set_res->supportedValues, nullptr);
                    if(nullptr != set_res->supportedValues) {
                        EXPECT_EQ(set_res->supportedValues->type, C2FieldSupportedValues::RANGE);
                        EXPECT_EQ(set_res->supportedValues->range.min.u32, 1);
                        EXPECT_EQ(set_res->supportedValues->range.max.u32, 51);
                        EXPECT_EQ(set_res->supportedValues->range.step.u32, 1);
                        EXPECT_EQ(set_res->supportedValues->range.nom.u32, 1);
                        EXPECT_EQ(set_res->supportedValues->range.denom.u32, 1);
                    }
                    EXPECT_TRUE(set_res->conflictingFields.empty());
                }
            }

            EncoderConsumer::OnFrame on_frame =
                [&] (const uint8_t* data, size_t length) {
                bitstream.PushBack(data, length);
                bitstream_len += length;
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            // validator checks that encoder correctly behaves on the changed config
            Encode(FRAME_COUNT, comp, validator, { &stripe_generator, &noise_generator } );

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
    }); // ForEveryComponent
}

// Queries param values and verify correct defaults.
// Does check before encoding (STOPPED state), during encoding on every frame,
// and after encoding.
TEST(MfxEncoderComponent, query_nb)
{
    GetComponentsCache().clear(); // reset cache to re-create components and have default params there

    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [&] (const ComponentDesc& comp_desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        (void)comp;

        auto check_default_values = [&] () {
            // check query through stack placeholders and the same with heap allocated
            std::vector<std::unique_ptr<C2Param>> heap_params;
            const C2ParamValues& default_values = comp_desc.default_values;
            status_t res = comp_intf->query_nb(default_values.GetStackPointers(),
                default_values.GetIndices(), &heap_params);
            EXPECT_EQ(res, comp_desc.query_status);

            default_values.CheckStackValues();
            default_values.Check(heap_params, true);
        };

        {
            SCOPED_TRACE("Before encode");
            check_default_values();
        }

        StripeGenerator stripe_generator;

        EncoderConsumer::OnFrame on_frame = [&] (const uint8_t*, size_t) {
            SCOPED_TRACE("During encode");
            check_default_values();
        };

        std::shared_ptr<EncoderConsumer> validator =
            std::make_shared<EncoderConsumer>(on_frame);

        Encode(FRAME_COUNT, comp, validator, { &stripe_generator } );

        {
            SCOPED_TRACE("After encode");
            check_default_values();
        }
    }); // ForEveryComponent
}
