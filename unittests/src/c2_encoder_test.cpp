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

#include <set>
#include <future>
#include <iostream>

using namespace android;

const uint64_t FRAME_DURATION_US = 33333; // 30 fps
const uint32_t FRAME_WIDTH = 640;
const uint32_t FRAME_HEIGHT = 480;
const uint32_t FRAME_BUF_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3 / 2;

const uint32_t FRAME_FORMAT = 0; // nv12
const uint32_t FRAME_COUNT = 10;
const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

const uint32_t STRIPE_COUNT = 16;

struct YuvColor {
    uint8_t Y;
    uint8_t U;
    uint8_t V;
};

const YuvColor FRAME_STRIPE_COLORS[2] =
{
    { 20, 230, 20 }, // dark-blue
    { 150, 60, 230 } // bright-red
};

// Renders one row of striped image, in NV12 format.
// Stripes are binary figuring frame_index.
static void RenderStripedRow(uint32_t frame_index, uint32_t frame_width, bool luma, uint8_t* row)
{
    int x = 0;
    for(uint32_t s = 0; s < STRIPE_COUNT; ++s) {
        int stripe_right_edge = (s + 1) * (frame_width / 2) / STRIPE_COUNT; // in 2x2 blocks
        const YuvColor& color = FRAME_STRIPE_COLORS[frame_index & 1/*lower bit*/];

        for(; x < stripe_right_edge; ++x) {
            if(luma) {
                row[2 * x] = color.Y;
                row[2 * x + 1] = color.Y;
            } else {
                row[2 * x] = color.U;
                row[2 * x + 1] = color.V;
            }
        }

        frame_index >>= 1; // next bit
    }
}

struct ComponentDesc
{
    const char* component_name;
    int flags;
    status_t creation_status;
};

static ComponentDesc g_components_desc[] = {
    { "C2.h264ve", 0, C2_OK },
    { "C2.NonExistingEncoder", 0, C2_NOT_FOUND },
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

static std::shared_ptr<MfxC2Component> GetCachedComponent(const char* name)
{
    static std::map<std::string, std::shared_ptr<MfxC2Component>> g_components;

    std::shared_ptr<MfxC2Component> result;

    auto it = g_components.find(name);
    if(it != g_components.end()) {
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

            g_components.emplace(name, result);
        }
    }
    return result;
}

TEST(MfxEncoderComponent, Create)
{
    for(const auto& desc : g_components_desc) {

        std::shared_ptr<MfxC2Component> encoder = GetCachedComponent(desc.component_name);

        EXPECT_EQ(encoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;
    }
}

TEST(MfxEncoderComponent, intf)
{
    for(const auto& desc : g_components_desc) {
        std::shared_ptr<MfxC2Component> encoder = GetCachedComponent(desc.component_name);
        EXPECT_EQ(encoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;;

        if(encoder != nullptr) {
            std::shared_ptr<C2Component> c2_component = encoder;
            std::shared_ptr<C2ComponentInterface> c2_component_intf = c2_component->intf();

            EXPECT_NE(c2_component_intf, nullptr);

            if(c2_component_intf != nullptr) {
                EXPECT_EQ(c2_component_intf->getName(), desc.component_name);
            }
        }
    }
}

static void PrepareWork(uint32_t frame_index, std::unique_ptr<C2Work>* work)
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

        if(nullptr == data) break;

        const uint32_t stride = FRAME_WIDTH;

        uint8_t* top_row = data;
        RenderStripedRow(frame_index, FRAME_WIDTH, true, top_row); // fill 1st luma row
        uint8_t* row = top_row + stride;
        for(uint32_t i = 1; i < FRAME_HEIGHT; ++i) {
            memcpy(row, top_row, stride); // copy top_row down the frame
            row += stride;
        }

        top_row = data + FRAME_HEIGHT * stride;
        RenderStripedRow(frame_index, FRAME_WIDTH, false, top_row); // fill 1st chroma row
        row = top_row + stride;
        for(uint32_t i = 1; i < FRAME_HEIGHT / 2; ++i) {
            memcpy(row, top_row, stride); // copy top_row down the frame
            row += stride;
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
    std::shared_ptr<C2Component> component,
    std::shared_ptr<EncoderConsumer> validator)
{
    component->registerListener(validator);

    status_t sts = component->start();
    EXPECT_EQ(sts, C2_OK);

    for(uint32_t frame_index = 0; frame_index < FRAME_COUNT; ++frame_index) {
        // prepare worklet and push
        std::unique_ptr<C2Work> work;

        // insert input data
        PrepareWork(frame_index, &work);
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
TEST(MfxEncoderComponent, EncodeBitExact)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        const int TESTS_COUNT = 5;
        BinaryChunks binary[TESTS_COUNT];

        for(int i = 0; i < TESTS_COUNT; ++i) {

            GTestBinaryWriter writer(std::ostringstream() << comp_intf->getName() << "-" << i << ".out");

            EncoderConsumer::OnFrame on_frame = [&] (const uint8_t* data, size_t length) {
                writer.Write(data, length);
                binary[i].PushBack(data, length);
            };

            std::shared_ptr<EncoderConsumer> validator =
                std::make_shared<EncoderConsumer>(on_frame);

            Encode(comp, validator);
        }
        // Every pair of results should be equal
        for (int i = 0; i < TESTS_COUNT - 1; ++i) {
            for (int j = i + 1; j < TESTS_COUNT; ++j) {
                EXPECT_EQ(binary[i], binary[j]) << "Pass " << i << " not equal to " << j;
            }
        }
    } );
}

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
