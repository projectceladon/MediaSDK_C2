/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "gtest_emulation.h"
#include "mfx_cmd_queue.h"
#include "mfx_gralloc_allocator.h"
#include <map>

using namespace android;

static const size_t CMD_COUNT = 10;

// Tests abstract command queue processed all supplied tasks in correct order.
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

// Tests that MfxCmdQueue::Stop is waiting for the end of all pushed tasks.
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

// Tests that MfxCmdQueue::Abort is not waiting for the end of all pushed tasks.
// At least some tasks should not be processed.
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

static void CheckNV12PlaneLayout(uint16_t width, uint16_t height, const C2PlaneLayout& layout)
{
    using Layout = C2PlaneLayout;
    using Info = C2PlaneInfo;

    EXPECT_EQ(layout.mType, Layout::MEDIA_IMAGE_TYPE_YUV);
    EXPECT_EQ(layout.mNumPlanes, 3);

    std::map<Layout::PlaneIndex, Info::Channel> expected_channels = {
        {  Layout::Y, Info::Y },
        {  Layout::U, Info::Cb },
        {  Layout::V, Info::Cr },
    };

    for (Layout::PlaneIndex index : { Layout::Y, Layout::U, Layout::V }) {
        EXPECT_EQ(layout.mPlanes[index].mChannel, expected_channels[index]);
        EXPECT_EQ(layout.mPlanes[index].mColInc, index == Layout::Y ? 1 : 2);
        EXPECT_TRUE(layout.mPlanes[index].mRowInc >= width);
        EXPECT_EQ(layout.mPlanes[index].mHorizSubsampling, index == Layout::Y ? 1 : 2);
        EXPECT_EQ(layout.mPlanes[index].mVertSubsampling, index == Layout::Y ? 1 : 2);
        EXPECT_EQ(layout.mPlanes[index].mBitDepth, 8);
        EXPECT_EQ(layout.mPlanes[index].mAllocatedDepth, 8);

        if (index != Layout::Y) EXPECT_TRUE(layout.mPlanes[index].mOffset >= width * height);
    }
    EXPECT_EQ(layout.mPlanes[Layout::Y].mOffset, 0);
    EXPECT_EQ(layout.mPlanes[Layout::U].mOffset + 1, layout.mPlanes[Layout::V].mOffset);
}

static uint8_t PlanePixelValue(uint16_t x, uint16_t y, uint32_t plane_index, int frame_index)
{
    return (uint8_t)(x + y + plane_index + frame_index);
}

typedef std::function<void(uint16_t x, uint16_t y, uint32_t plane_index, uint8_t* plane_pixel)> ProcessPlanePixel;

static void ForEveryPlanePixel(uint16_t width, uint16_t height, const C2PlaneLayout& layout,
    const ProcessPlanePixel& process_function, uint8_t* data)
{
    for (uint32_t i = 0; i < layout.mNumPlanes; ++i) {
        const C2PlaneInfo& plane = layout.mPlanes[i];

        uint8_t* row = data + plane.mOffset;
        for (uint16_t y = 0; y < height; y += plane.mVertSubsampling) {
            uint8_t* pixel = row;
            for (uint16_t x = 0; x < width; x += plane.mHorizSubsampling) {
                process_function(x, y, i, pixel);
                pixel += plane.mColInc;
            }
            row += plane.mRowInc;
        }
    }
}

// Fills frame planes with PlanePixelValue pattern.
// Value should depend on plane index, frame index, x and y.
static void FillFrameContents(uint16_t width, uint16_t height, int frame_index,
    const C2PlaneLayout& layout, uint8_t* data)
{
    ProcessPlanePixel process = [frame_index] (uint16_t x, uint16_t y, uint32_t plane_index, uint8_t* plane_pixel) {
        *plane_pixel = PlanePixelValue(x, y, plane_index, frame_index);
    };
    ForEveryPlanePixel(width, height, layout, process, data);
}

static void CheckFrameContents(uint16_t width, uint16_t height, int frame_index,
    const C2PlaneLayout& layout, uint8_t* data)
{
    int fails_count = 0;

    ProcessPlanePixel process = [frame_index, &fails_count] (uint16_t x, uint16_t y, uint32_t plane_index, uint8_t* plane_pixel) {
        if (fails_count < 10) { // to not overflood output
            bool match = (*plane_pixel == PlanePixelValue(x, y, plane_index, frame_index));
            if (!match) ++fails_count;
            EXPECT_TRUE(match) << NAMED(x) << NAMED(y) << NAMED(plane_index);
        }
    };
    ForEveryPlanePixel(width, height, layout, process, data);
}

// Tests gralloc allocator ability to alloc and free buffers.
// The allocated buffer is locked, filled memory with some pattern,
// unlocked, then locked again, memory pattern should the same.
TEST(MfxGrallocAllocator, BufferKeepsContents)
{
    std::unique_ptr<MfxGrallocAllocator> allocator;

    status_t res = MfxGrallocAllocator::Create(&allocator);
    EXPECT_EQ(res, C2_OK);
    EXPECT_NE(allocator, nullptr);

    const int WIDTH = 600;
    const int HEIGHT = 400;
    const int FRAME_COUNT = 3;

    if (allocator) {

        buffer_handle_t handle[FRAME_COUNT] {};
        status_t res;

        for (int i = 0; i < FRAME_COUNT; ++i) {
            res = allocator->Alloc(WIDTH, HEIGHT, &handle[i]);
            EXPECT_EQ(res, C2_OK);
            EXPECT_NE(handle, nullptr);
        }

        for (int i = 0; i < FRAME_COUNT; ++i) {
            uint8_t* data {};
            android::C2PlaneLayout layout {};
            res = allocator->LockFrame(handle[i], &data, &layout);
            EXPECT_EQ(res, C2_OK);
            EXPECT_NE(data, nullptr);

            CheckNV12PlaneLayout(WIDTH, HEIGHT, layout);

            FillFrameContents(WIDTH, HEIGHT, i, layout, data);

            res = allocator->UnlockFrame(handle[i]);
            EXPECT_EQ(res, C2_OK);
        }

        for (int i = 0; i < FRAME_COUNT; ++i) {
            uint8_t* data {};
            android::C2PlaneLayout layout {};
            res = allocator->LockFrame(handle[i], &data, &layout);
            EXPECT_EQ(res, C2_OK);
            EXPECT_NE(data, nullptr);

            CheckNV12PlaneLayout(WIDTH, HEIGHT, layout);

            CheckFrameContents(WIDTH, HEIGHT, i, layout, data);

            res = allocator->UnlockFrame(handle[i]);
            EXPECT_EQ(res, C2_OK);
        }

        for (int i = 0; i < FRAME_COUNT; ++i) {
            res = allocator->Free(handle[i]);
            EXPECT_EQ(res, C2_OK);
        }
    }
}
