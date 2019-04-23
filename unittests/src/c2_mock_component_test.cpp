/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <dlfcn.h>

#include "mfx_c2_defs.h"
#include "mfx_c2_utils.h"
#include <gtest/gtest.h>
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_mock_component.h"
#include "C2PlatformSupport.h"

#include <set>
#include <future>
#include <iostream>

using namespace android;

#define MOCK_COMPONENT_ENC "c2.intel.mock.encoder"
#define MOCK_COMPONENT_DEC "c2.intel.mock.decoder"
#define MOCK_COMPONENT MOCK_COMPONENT_ENC // use encoder for common tests

const uint64_t FRAME_DURATION_US = 33333; // 30 fps
const uint32_t FRAME_WIDTH = 640;
const uint32_t FRAME_HEIGHT = 480;
const uint32_t FRAME_BUF_SIZE = FRAME_WIDTH * FRAME_HEIGHT * 3 / 2;

const uint32_t FRAME_FORMAT = HAL_PIXEL_FORMAT_NV12_TILED_INTEL; // nv12
const uint32_t FRAME_COUNT = 10;
const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

// Tests if the mock component is created OK.
TEST(MfxMockComponent, Create)
{
    int flags = 0;
    c2_status_t sts = C2_OK;
    std::shared_ptr<MfxC2Component> mfx_component(MfxCreateC2Component(MOCK_COMPONENT, flags, &sts));

    EXPECT_EQ(sts, C2_OK);
    EXPECT_NE(mfx_component, nullptr);
}

// Tests mock component expose C2ComponentInterface
// and return correct information once queried (component name).
TEST(MfxMockComponent, intf)
{
    int flags = 0;
    c2_status_t sts = C2_OK;
    std::shared_ptr<MfxC2Component> mfx_component(MfxCreateC2Component(MOCK_COMPONENT, flags, &sts));

    EXPECT_EQ(sts, C2_OK);
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

// Allocates c2 graphic block of FRAME_WIDTH x FRAME_HEIGHT size and fills it with
// specified byte value.
static std::unique_ptr<C2ConstGraphicBlock> CreateFilledGraphicBlock(
    std::shared_ptr<C2BlockPool> allocator, uint8_t fill, uint64_t consumer_memory_type)
{
    std::unique_ptr<C2ConstGraphicBlock> res;

    do {
        C2MemoryUsage mem_usage = { consumer_memory_type, C2MemoryUsage::CPU_WRITE };
        std::shared_ptr<C2GraphicBlock> block;
        c2_status_t sts = allocator->fetchGraphicBlock(FRAME_WIDTH, FRAME_HEIGHT, FRAME_FORMAT,
            mem_usage, &block);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(block, nullptr);

        if(nullptr == block) break;
        {
            std::unique_ptr<C2GraphicView> graph_view;
            sts = MapGraphicBlock(*block, TIMEOUT_NS, &graph_view);
            EXPECT_EQ(sts, C2_OK);
            EXPECT_NE(graph_view, nullptr);

            if(nullptr == graph_view) break;

            memset(graph_view->data()[0], fill, FRAME_BUF_SIZE);
        }
        C2Event event;
        event.fire(); // pre-fire as buffer is already ready to use
        res = std::make_unique<C2ConstGraphicBlock>(block->share(block->crop(), event.fence()));

    } while(false);

    return res;
}

// Allocates c2 linear block of FRAME_BUF_SIZE length and fills it with
// specified byte value.
static std::unique_ptr<C2ConstLinearBlock> CreateFilledLinearBlock(
    std::shared_ptr<C2BlockPool> allocator, uint8_t fill)
{
    std::unique_ptr<C2ConstLinearBlock> res;

    do {
        C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        std::shared_ptr<C2LinearBlock> block;
        c2_status_t sts = allocator->fetchLinearBlock(FRAME_BUF_SIZE, mem_usage, &block);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(block, nullptr);

        if(nullptr == block) break;

        std::unique_ptr<C2WriteView> write_view;
        sts = MapLinearBlock(*block, TIMEOUT_NS, &write_view);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(write_view, nullptr);

        if(nullptr == write_view) break;

        uint8_t* data = write_view->data();
        EXPECT_NE(data, nullptr);

        if(nullptr == data) break;

        memset(data, fill, FRAME_BUF_SIZE);

        C2Event event;
        event.fire(); // pre-fire as buffer is already ready to use
        res = std::make_unique<C2ConstLinearBlock>(block->share(0, block->capacity(), event.fence()));

    } while(false);

    return res;
}

// Prepares C2Work filling it with NV12 frame.
// Frame size is (FRAME_WIDTH x FRAME_HEIGHT).
// Frame buffer size is (FRAME_WIDTH * FRAME_HEIGHT * 3 / 2).
// Each byte in NV12 frame is set to frame_index.
// Frame header index and timestamp are set based on passed frame_index value.
static void PrepareWork(uint32_t frame_index,
    std::shared_ptr<const C2Component> component,
    std::unique_ptr<C2Work>* work,
    C2BufferData::Type buffer_type, uint64_t consumer_memory_type)
{
    *work = std::make_unique<C2Work>();
    C2FrameData* buffer_pack = &((*work)->input);

    if(frame_index < FRAME_COUNT - 1) {
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
        auto block_pool_id = (buffer_type == C2BufferData::LINEAR) ?
            C2BlockPool::BASIC_LINEAR : C2BlockPool::BASIC_GRAPHIC;
        c2_status_t sts = GetCodec2BlockPool(block_pool_id,
            component, &allocator);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(allocator, nullptr);

        if(nullptr == allocator) break;

        std::shared_ptr<C2Buffer> buffer;
        // fill the frame with pixels == frame_index
        if(buffer_type == C2BufferData::GRAPHIC) {
            std::unique_ptr<C2ConstGraphicBlock> const_block =
                CreateFilledGraphicBlock(allocator, (uint8_t)frame_index, consumer_memory_type);
            if(nullptr == const_block) break;
            // make buffer of graphic block
            buffer = std::make_shared<C2Buffer>(MakeC2Buffer( { *const_block } ));
        } else {
            std::unique_ptr<C2ConstLinearBlock> const_block = CreateFilledLinearBlock(allocator, (uint8_t)frame_index);
            if(nullptr == const_block) break;
            // make buffer of linear block
            buffer = std::make_shared<C2Buffer>(MakeC2Buffer( { *const_block } ));
        }

        buffer_pack->buffers.push_back(buffer);

        std::unique_ptr<C2Worklet> worklet = std::make_unique<C2Worklet>();
        // C2 requires output items be allocated in buffers list and set to nulls
        worklet->output.buffers.push_back(nullptr);
        // work of 1 worklet
        (*work)->worklets.push_back(std::move(worklet));
    }
    while(false);
}

static void CheckFilledBuffer(const uint8_t* raw, int expected_item)
{
    if(nullptr != raw) {

        bool match = true;
        for(uint32_t i = 0; i < FRAME_BUF_SIZE; ++i) {
             // all bytes in buffer should be equal to frame index
            if(raw[i] != expected_item) {
                match = false;
                break;
            }
        }
        EXPECT_EQ(match, true);
    }
}

class MockOutputValidator : public C2Component::Listener
{
public:
    MockOutputValidator(C2BufferData::Type output_type)
        : output_type_(output_type)
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
            EXPECT_EQ(work->workletsProcessed, 1u);
            EXPECT_EQ(work->result, C2_OK);
            EXPECT_EQ(work->worklets.size(), 1u);
            if (work->worklets.size() >= 1) {

                std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
                C2FrameData& buffer_pack = worklet->output;

                uint64_t frame_index = buffer_pack.ordinal.frameIndex.peeku();

                bool last_frame = (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) != 0;
                EXPECT_EQ(buffer_pack.flags, last_frame ? C2FrameData::FLAG_END_OF_STREAM : C2FrameData::flags_t{});
                EXPECT_EQ(buffer_pack.ordinal.timestamp, frame_index * FRAME_DURATION_US); // 30 fps

                EXPECT_EQ(frame_index < FRAME_COUNT, true)
                    << "unexpected frame_index value" << frame_index;
                {
                    std::lock_guard<std::mutex> lock(expectations_mutex_);
                    EXPECT_EQ(frame_index, frame_expected_)
                        << " frame " << frame_index << " is out of order";
                    ++frame_expected_;
                }

                std::unique_ptr<C2ConstLinearBlock> linear_block;
                std::unique_ptr<C2ConstGraphicBlock> graphic_block;

                if(output_type_ == C2BufferData::LINEAR) {
                    c2_status_t sts = GetC2ConstLinearBlock(buffer_pack, &linear_block);
                    EXPECT_EQ(sts, C2_OK);
                    if(nullptr != linear_block) {
                        EXPECT_EQ(linear_block->capacity(), FRAME_BUF_SIZE);

                        std::unique_ptr<C2ReadView> read_view;
                        sts = MapConstLinearBlock(*linear_block, TIMEOUT_NS, &read_view);
                        EXPECT_NE(read_view, nullptr);

                        const uint8_t* raw = read_view->data();
                        EXPECT_EQ(sts, C2_OK);
                        EXPECT_NE(raw, nullptr);

                        CheckFilledBuffer(raw, frame_index);
                    }
                } else if(output_type_ == C2BufferData::GRAPHIC) {
                    c2_status_t sts = GetC2ConstGraphicBlock(buffer_pack, &graphic_block);
                    EXPECT_EQ(sts, C2_OK);
                    if(nullptr != graphic_block) {
                        EXPECT_EQ(graphic_block->width(), FRAME_WIDTH);
                        EXPECT_EQ(graphic_block->height(), FRAME_HEIGHT);

                        std::unique_ptr<const C2GraphicView> c_graph_view;
                        sts = MapConstGraphicBlock(*graphic_block, TIMEOUT_NS, &c_graph_view);
                        EXPECT_EQ(sts, C2_OK);
                        const uint8_t* const* raw = c_graph_view->data();
                        EXPECT_NE(raw, nullptr);
                        CheckFilledBuffer(raw[0], frame_index);
                    }
                } else {
                    ADD_FAILURE() << "unexpected value of output_type_";
                }
            }
        }
        {
            std::lock_guard<std::mutex> lock(expectations_mutex_);
            // if collected all expected frames
            if(frame_expected_ >= FRAME_COUNT) {
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

public:
    std::mutex expectations_mutex_;
    uint64_t frame_expected_ = 0; // frame index is next to come
    C2BufferData::Type output_type_;
    std::promise<void> done_; // fire when all expected frames came
};

// Tests how the mock component processes a sequence of C2Work items, in encoder way.
// It accepts c2 frame buffers and allocates output of c2 linear buffer the same size.
// The component copies input buffer to output without any changes.
// The test checks that order of inputs is not changed
// and output is accurately the same as input.
// Also the component processing should make output within 10 seconds (test on hang).
// All supplementary entities (c2 buffers and command queues) are tested by this test.
TEST(MfxMockComponent, Encode)
{
    int flags = 0;
    c2_status_t sts = C2_OK;
    std::shared_ptr<C2Component> component(MfxCreateC2Component(MOCK_COMPONENT_ENC, flags, &sts));

    EXPECT_EQ(sts, C2_OK);
    EXPECT_NE(component, nullptr);
    if(nullptr != component) {

        for (uint64_t consumer_memory_type :
            { (uint64_t)C2MemoryUsage::CPU_READ, (uint64_t)android::C2AndroidMemoryUsage::HW_CODEC_READ } ) {

            std::shared_ptr<MockOutputValidator> validator =
                std::make_unique<MockOutputValidator>(C2BufferData::LINEAR);
            c2_blocking_t may_block {};
            component->setListener_vb(validator, may_block);

            sts = component->start();
            EXPECT_EQ(sts, C2_OK);

            for(uint32_t frame_index = 0; frame_index < FRAME_COUNT; ++frame_index) {
                // prepare worklet and push
                std::unique_ptr<C2Work> work;

                // insert input data
                PrepareWork(frame_index, component, &work, C2BufferData::GRAPHIC, consumer_memory_type);
                std::list<std::unique_ptr<C2Work>> works;
                works.push_back(std::move(work));

                sts = component->queue_nb(&works);
                EXPECT_EQ(sts, C2_OK);
            }

            std::future<void> future = validator->GetFuture();
            std::future_status future_sts = future.wait_for(std::chrono::seconds(10));
            EXPECT_EQ(future_sts, std::future_status::ready);

            component->setListener_vb(nullptr, may_block);
            sts = component->stop();
            EXPECT_EQ(sts, C2_OK);
            validator = nullptr;
        }
    }
}

// Tests how the mock component processes a sequence of C2Work items, in decoder way.
// It accepts c2 linear buffer and allocates c2 frame buffer length >= of input.
// The component copies input buffer to output without any changes.
// Leftover of output, if any, is filled with zeroes.
// The test checks that order of inputs is not changed
// and output is accurately the same as input.
// Also the component processing should make output within 10 seconds (test on hang).
// All supplementary entities (c2 buffers and command queues) are tested by this test.
TEST(MfxMockComponent, Decode)
{
    int flags = 0;
    c2_status_t sts = C2_OK;
    std::shared_ptr<C2Component> component(MfxCreateC2Component(MOCK_COMPONENT_DEC, flags, &sts));

    EXPECT_EQ(sts, C2_OK);
    EXPECT_NE(component, nullptr);
    if(nullptr != component) {

        for (uint64_t producer_memory_type :
            { (uint64_t)C2MemoryUsage::CPU_WRITE, (uint64_t)android::C2AndroidMemoryUsage::HW_CODEC_WRITE } ) {

            std::shared_ptr<MockOutputValidator> validator =
                std::make_unique<MockOutputValidator>(C2BufferData::GRAPHIC);
            c2_blocking_t may_block {};
            component->setListener_vb(validator, may_block);

            if(component != nullptr) {

                std::shared_ptr<C2ComponentInterface> component_intf = component->intf();
                EXPECT_NE(component_intf, nullptr);

                if (!component_intf) continue;

                C2ProducerMemoryType memory_type_setting(producer_memory_type);
                sts = component_intf->config_vb( { &memory_type_setting }, may_block, nullptr );
                EXPECT_EQ(sts, C2_OK);

                sts = component->start();
                EXPECT_EQ(sts, C2_OK);

                for(uint32_t frame_index = 0; frame_index < FRAME_COUNT; ++frame_index) {
                    // prepare worklet and push
                    std::unique_ptr<C2Work> work;

                    // insert input data
                    PrepareWork(frame_index, component, &work, C2BufferData::LINEAR, C2MemoryUsage::CPU_READ);
                    std::list<std::unique_ptr<C2Work>> works;
                    works.push_back(std::move(work));

                    sts = component->queue_nb(&works);
                    EXPECT_EQ(sts, C2_OK);
                }
            }

            std::future<void> future = validator->GetFuture();
            std::future_status future_sts = future.wait_for(std::chrono::seconds(10));
            EXPECT_EQ(future_sts, std::future_status::ready);

            component->setListener_vb(nullptr, may_block);
            sts = component->stop();
            EXPECT_EQ(sts, C2_OK);
            validator = nullptr;
        }
    }
}

class C2ComponentStateListener : public C2Component::Listener
{
private:
    void onWorkDone_nb(std::weak_ptr<C2Component>, std::list<std::unique_ptr<C2Work>>) {}

    void onTripped_nb(std::weak_ptr<C2Component>,
        std::vector<std::shared_ptr<C2SettingResult>>)
    {
        tripped_notified_.set_value();
    }

    void onError_nb(std::weak_ptr<C2Component>, uint32_t)
    {
        error_notified_.set_value();
    }

    bool WaitFor(std::promise<void>* promise)
    {
        bool result = (std::future_status::ready ==
            promise->get_future().wait_for(std::chrono::seconds(1)));
        *promise = std::promise<void>(); // prepare for next notification
        return result;
    }

private:
    std::promise<void> tripped_notified_;

    std::promise<void> error_notified_;

public:
    bool WaitTrippedState() { return WaitFor(&tripped_notified_); }

    bool WaitErrorState() { return WaitFor(&error_notified_); }
};

// Checks the correctness of mock component state machine.
// The component should be able to start from STOPPED (initial) state,
// stop from RUNNING state. Otherwise, C2_BAD_STATE should be returned.
TEST(MfxMockComponent, State)
{
    int flags = 0;
    c2_status_t sts = C2_OK;
    std::shared_ptr<C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, flags, &sts));

    EXPECT_EQ(sts, C2_OK);
    EXPECT_NE(component, nullptr);
    if(nullptr != component) {

        sts = component->start();
        EXPECT_EQ(sts, C2_OK);

        sts = component->start();
        EXPECT_EQ(sts, C2_BAD_STATE);

        sts = component->release();
        EXPECT_EQ(sts, C2_OK);

        component = nullptr;

        std::shared_ptr<C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, flags, &sts));
        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(component, nullptr);

        if (nullptr != component) {
            std::shared_ptr<C2ComponentStateListener> state_listener =
                std::make_shared<C2ComponentStateListener>();
            sts = component->setListener_vb(state_listener, c2_blocking_t{});

            sts = component->start();
            EXPECT_EQ(sts, C2_OK);

            sts = component->stop();
            EXPECT_EQ(sts, C2_OK);

            sts = component->stop();
            EXPECT_EQ(sts, C2_BAD_STATE);

            sts = component->start();
            EXPECT_EQ(sts, C2_OK);

            auto trip_component = [&] () {
                C2TrippedTuning tripped_tuning;
                tripped_tuning.value = C2_TRUE;
                c2_blocking_t may_block{};
                sts = component->intf()->config_vb({&tripped_tuning}, may_block, nullptr);
                EXPECT_EQ(sts, C2_OK);
            };

            trip_component();
            EXPECT_TRUE(state_listener->WaitTrippedState());

            sts = component->start();
            EXPECT_EQ(sts, C2_OK);

            trip_component();
            EXPECT_TRUE(state_listener->WaitTrippedState());

            sts = component->stop();
            EXPECT_EQ(sts, C2_OK);

            sts = component->start();
            EXPECT_EQ(sts, C2_OK);

            auto fail_component = [&] () {
                std::list<std::unique_ptr<C2Work>> null_work;
                null_work.push_back(nullptr);

                sts = component->queue_nb(&null_work); // cause component error
                EXPECT_EQ(sts, C2_OK);
            };

            fail_component();
            EXPECT_TRUE(state_listener->WaitErrorState());

            sts = component->start();
            EXPECT_EQ(sts, C2_BAD_STATE);

            sts = component->stop();
            EXPECT_EQ(sts, C2_OK);

            // Next start, fail, stop series to check for deadlock.
            // stop right after enqueuing work leads component to process the work
            // during stop operation, so state change (to ERROR) takes place while
            // changing to STOPPED. That caused deadlock on MfxC2Component::state_mutex_.
            // To prevent this MfxC2Component::next_state_ was introduced.
            sts = component->start();
            EXPECT_EQ(sts, C2_OK);

            fail_component();

            sts = component->stop();
            EXPECT_EQ(sts, C2_OK);

            sts = component->release();
            EXPECT_EQ(sts, C2_OK);

            sts = component->release();
            EXPECT_EQ(sts, C2_DUPLICATE);
        }
    }
}

// Tests mayBlock option handling of config_vb and query_vb methods.
TEST(MfxMockComponent, ConfigQueryBlocking)
{
    int flags = 0;
    c2_status_t sts = C2_OK;
    std::shared_ptr<C2Component> component(MfxCreateC2Component(MOCK_COMPONENT_DEC, flags, &sts));
    std::shared_ptr<C2ComponentInterface> component_intf;

    EXPECT_EQ(sts, C2_OK);
    EXPECT_NE(component, nullptr);
    if (nullptr != component) {
        component_intf = component->intf();
        EXPECT_NE(component_intf, nullptr);
    }

    if (component_intf) {

        // Calls config_vb and query_vb methods with specified blocking and checking result to be expected
        auto test_config_query = [component_intf] (c2_blocking_t blocking, c2_status_t expected) {
            C2ProducerMemoryType some_setting{C2MemoryUsage::CPU_WRITE}; // any setting supported by the component
            c2_status_t sts = component_intf->config_vb({&some_setting}, blocking, nullptr);
            EXPECT_EQ(sts, expected);
            sts = component_intf->query_vb({&some_setting}, {}, blocking, {});
            EXPECT_EQ(sts, expected);
        };

        // This class implements listener interface
        // and hangs in onWorkDone callback blocking C2Component from handling works.
        // It hangs stop() method too as stop() tries to complete queued works.
        // It allows handling on Unblock call.
        class Blocker : public C2Component::Listener
        {
        private:
            void onWorkDone_nb(std::weak_ptr<C2Component>, std::list<std::unique_ptr<C2Work>>) override
            {
                // As lock_ is pre-locked on mutex_  this line blocks execution
                // until Unblock method is called.
                std::lock_guard<std::mutex> lock(mutex_);
            }
            void onTripped_nb(std::weak_ptr<C2Component>,
                std::vector<std::shared_ptr<C2SettingResult>>) override {}

            void onError_nb(std::weak_ptr<C2Component>, uint32_t) override {}
        private:
            std::mutex mutex_;
            std::unique_lock<std::mutex> lock_{mutex_}; // it constructs lock_ locked on mutex_
        public:
            void Unblock() {
                lock_.unlock();
            }
        };
        std::shared_ptr<Blocker> blocker = std::make_shared<Blocker>();

        sts = component->setListener_vb(blocker, C2_DONT_BLOCK/*option for setting this listener*/);
        EXPECT_EQ(sts, C2_OK);

        sts = component->start();
        EXPECT_EQ(sts, C2_OK);

        for(uint32_t frame_index = 0; frame_index < FRAME_COUNT; ++frame_index) {
            // prepare worklet and push
            std::unique_ptr<C2Work> work;

            // insert input data
            PrepareWork(frame_index, component, &work, C2BufferData::LINEAR, C2MemoryUsage::CPU_READ);
            std::list<std::unique_ptr<C2Work>> works;
            works.push_back(std::move(work));

            sts = component->queue_nb(&works);
            EXPECT_EQ(sts, C2_OK);
        }

        // Call config_vb/query_vb with C2_DONT_BLOCK while the component is in stable (RUNNING) state,
        // no internal state blocking expected, methods should be able to run momentarily, without errors.
        test_config_query(C2_DONT_BLOCK, C2_OK);

        // Run stop and simultanouesly test config_vb/query_vb methods. stop should finish enqueued tasks
        // what takes some time to switch to STOPPED state. That transition blocks config_vb/query_vb from execution.
        // This means that config_vb/query_vb should fail with C2_DONT_BLOCK option and succeed with C2_MAY_BLOCK.
        std::thread another_thread([&] () {

            C2ProducerMemoryType some_setting{C2MemoryUsage::CPU_WRITE}; // any setting supported by the component
            // If query_vb is OK, state is not transition, stop() not started yet
            while (C2_OK == component_intf->query_vb({&some_setting}, {}, C2_DONT_BLOCK, {})) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            test_config_query(C2_DONT_BLOCK, C2_BLOCKING);
            blocker->Unblock(); // Allow stop() to be continued
            test_config_query(C2_MAY_BLOCK, C2_OK);
        });
        sts = component->stop();
        EXPECT_EQ(sts, C2_OK);

        another_thread.join();
    }
}
