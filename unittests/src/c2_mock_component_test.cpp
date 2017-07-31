/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <dlfcn.h>

#include "mfx_c2_defs.h"
#include "mfx_c2_utils.h"
#include "gtest_emulation.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_mock_component.h"
#include "C2BlockAllocator.h"

#include <set>
#include <future>
#include <iostream>

using namespace android;

#define MOCK_COMPONENT "C2.MockComponent"

const uint64_t FRAME_DURATION_US = 33333; // 30 fps
const uint32_t FRAME_WIDTH = 640;
const uint32_t FRAME_HEIGHT = 480;
const uint32_t FRAME_BUF_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3 / 2;

const uint32_t FRAME_FORMAT = 0; // nv12
const uint32_t FRAME_COUNT = 10;
const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;


TEST(MfxMockComponent, Create)
{
    int flags = 0;
    MfxC2Component* c_mfx_component;
    status_t result = MfxCreateC2Component(MOCK_COMPONENT, flags, &c_mfx_component);
    std::shared_ptr<MfxC2Component> mfx_component(c_mfx_component);
    EXPECT_EQ(result, C2_OK);
    EXPECT_NE(mfx_component, nullptr);
}

TEST(MfxMockComponent, intf)
{
    int flags = 0;
    MfxC2Component* c_mfx_component;
    status_t result = MfxCreateC2Component(MOCK_COMPONENT, flags, &c_mfx_component);
    std::shared_ptr<MfxC2Component> mfx_component(c_mfx_component);

    EXPECT_NE(mfx_component, nullptr);
    if(mfx_component != nullptr) {
        std::shared_ptr<C2Component> c2_component = mfx_component;
        std::shared_ptr<C2ComponentInterface> c2_component_intf = c2_component->intf();

        EXPECT_NE(c2_component_intf, nullptr);
        if(c2_component_intf != nullptr) {
            EXPECT_EQ(c2_component_intf->getName(), MOCK_COMPONENT);
        }
    }
}

// Prepares C2Work filling it with NV12 frame.
// Frame size is (FRAME_WIDTH x FRAME_HEIGHT).
// Frame buffer size is (FRAME_WIDTH * FRAME_HEIGHT * 3 / 2).
// Each byte in NV12 frame is set to frame_index.
// Frame header index and timestamp are set based on passed frame_index value.
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

        // fill the frame with pixels == frame_index
        memset(data, frame_index, FRAME_BUF_SIZE);

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
    }
    while(false);
}

class MockOutputValidator : public C2ComponentListener
{
public:
    MockOutputValidator() = default;
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
            EXPECT_EQ(linear_block->capacity(), FRAME_BUF_SIZE);

            const uint8_t* raw {};

            sts = MapConstLinearBlock(*linear_block, TIMEOUT_NS, &raw);
            EXPECT_EQ(sts, C2_OK);
            EXPECT_NE(raw, nullptr);

            if(nullptr != raw) {

                bool match = true;
                for(uint32_t i = 0; i < FRAME_BUF_SIZE; ++i) {
                     // all bytes in buffer should be equal to frame index
                    if(raw[i] != frame_index) {
                        match = false;
                        break;
                    }
                }
                EXPECT_EQ(match, true);
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

public:
    uint64_t frame_expected_; // frame index is next to come
    std::promise<void> done_; // fire when all expected frames came
};

TEST(MfxMockComponent, Process)
{
    status_t sts = C2_OK;
    MfxC2Component* c_mfx_component;
    status_t result = MfxCreateC2Component(MOCK_COMPONENT, 0/*flags*/, &c_mfx_component);
    std::shared_ptr<C2Component> component(c_mfx_component);
    EXPECT_NE(component, nullptr);
    if(nullptr != component) {

        std::shared_ptr<MockOutputValidator> validator = std::make_unique<MockOutputValidator>();
        component->registerListener(validator);

        if(component != nullptr) {
            sts = component->start();
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
        }

        std::future<void> future = validator->GetFuture();
        std::future_status future_sts = future.wait_for(std::chrono::seconds(10));
        EXPECT_EQ(future_sts, std::future_status::ready);

        component->unregisterListener(validator);
        sts = component->stop();
        EXPECT_EQ(sts, C2_OK);
        validator = nullptr;
    }
}

TEST(MfxMockComponent, State)
{
    status_t sts = C2_OK;
    MfxC2Component* c_mfx_component;
    status_t result = MfxCreateC2Component(MOCK_COMPONENT, 0/*flags*/, &c_mfx_component);
    std::shared_ptr<C2Component> component(c_mfx_component);
    EXPECT_NE(component, nullptr);
    if(nullptr != component) {

        sts = component->start();
        EXPECT_EQ(sts, C2_OK);

        sts = component->start();
        EXPECT_EQ(sts, C2_BAD_STATE);

        sts = component->stop();
        EXPECT_EQ(sts, C2_OK);

        sts = component->stop();
        EXPECT_EQ(sts, C2_BAD_STATE);
    }
}
