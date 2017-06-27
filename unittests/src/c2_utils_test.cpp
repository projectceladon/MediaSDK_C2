/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "test_utils.h"
#include "mfx_cmd_queue.h"

static const size_t CMD_COUNT = 10;

TEST(MfxCmdQueue, ProcessAll)
{
    MfxCmdQueue queue;
    queue.Start();

    std::vector<size_t> result;

    for(size_t i = 0; i < CMD_COUNT; ++i) {

        std::unique_ptr<int> ptr_i = std::make_unique<int>(i);

        // lambda mutable and not copy-assignable to assert MfxCmdQueue supports it
        auto task = [ ptr_i = std::move(ptr_i), &result] () mutable {
            result.push_back(*ptr_i);
            ptr_i = nullptr;
        };

        queue.Push(std::move(task));
    }

    queue.Stop();

    EXPECT_EQ(result.size(), CMD_COUNT);
    for(size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], i);
    }
}

TEST(MfxCmdQueue, Stop)
{
    MfxCmdQueue queue;
    queue.Start();

    std::vector<size_t> result;

    auto timeout = std::chrono::milliseconds(1);
    for(size_t i = 0; i < CMD_COUNT; ++i) {

        queue.Push( [i, timeout, &result] {

            std::this_thread::sleep_for(timeout);
            result.push_back(i);
        } );

        // progressively increase timeout to be sure some of them will not be processed
        // by moment of Stop
        timeout *= 2;
    }

    queue.Stop();

    EXPECT_EQ(result.size(), CMD_COUNT); // all commands should be executed
    for(size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], i);
    }
}

TEST(MfxCmdQueue, Abort)
{
    MfxCmdQueue queue;
    queue.Start();

    std::vector<size_t> result;

    auto timeout = std::chrono::milliseconds(1);
    for(size_t i = 0; i < CMD_COUNT; ++i) {

        queue.Push( [i, timeout, &result] {

            std::this_thread::sleep_for(timeout);
            result.push_back(i);
        } );

        // progressively increase timeout to be sure some of them will not be processed
        timeout *= 2;
    }

    queue.Abort();

    EXPECT_EQ(result.size() < CMD_COUNT, true); // some commands must be dropped
    for(size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], i);
    }
}
