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
#include "mfx_va_allocator.h"
#include "mfx_dev_va.h"
#include <map>
#include <set>

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

static void CheckNV12MfxFrameData(uint16_t width, uint16_t height,
    bool hw_memory, bool locked, const mfxFrameData& frame_data)
{
    EXPECT_EQ(frame_data.PitchHigh, 0);
    uint32_t pitch = MakeUint32(frame_data.PitchHigh, frame_data.PitchLow);

    EXPECT_TRUE(pitch >= width);

    EXPECT_EQ(frame_data.MemId != nullptr, hw_memory);

    bool pointers_expected = locked || !hw_memory;

    EXPECT_EQ(pointers_expected, frame_data.Y != nullptr);
    EXPECT_EQ(pointers_expected, frame_data.UV != nullptr);
    EXPECT_EQ(pointers_expected, frame_data.V != nullptr);

    if(pointers_expected) {
        EXPECT_TRUE(frame_data.Y + pitch * height <= frame_data.UV);
        EXPECT_EQ(frame_data.UV + 1, frame_data.V);
    }
    EXPECT_EQ(frame_data.A, nullptr);
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

static void ForEveryPlanePixel(uint16_t width, uint16_t height, const mfxFrameInfo& frame_info,
    const ProcessPlanePixel& process_function, const mfxFrameData& frame_data)
{
    EXPECT_EQ(frame_info.FourCC, MFX_FOURCC_NV12) << "only nv12 supported";
    EXPECT_EQ(frame_info.ChromaFormat, MFX_CHROMAFORMAT_YUV420) << "only chroma 420 supported";

    const int planes_count = 3;
    uint8_t* planes_data[planes_count] = { frame_data.Y, frame_data.UV, frame_data.UV + 1 };
    const uint16_t planes_vert_subsampling[planes_count] = { 1, 2, 2 };
    const uint16_t planes_horz_subsampling[planes_count] = { 1, 2, 2 };

    uint32_t pitch = MakeUint32(frame_data.PitchHigh, frame_data.PitchLow);
    const uint16_t planes_col_inc[planes_count] = { 1, 2, 2 };

    for (int i = 0; i < planes_count; ++i) {

        uint8_t* row = planes_data[i];
        for (uint16_t y = 0; y < height; y += planes_vert_subsampling[i]) {
            uint8_t* pixel = row;
            for (uint16_t x = 0; x < width; x += planes_horz_subsampling[i]) {
                process_function(x, y, i, pixel);
                pixel += planes_col_inc[i];
            }
            row += pitch;
        }
    }
}

// Fills frame planes with PlanePixelValue pattern.
// Value should depend on plane index, frame index, x and y.
template<typename FrameInfo, typename FrameData>
static void FillFrameContents(uint16_t width, uint16_t height, int frame_index,
    const FrameInfo& frame_info, const FrameData& frame_data)
{
    ProcessPlanePixel process = [frame_index] (uint16_t x, uint16_t y, uint32_t plane_index, uint8_t* plane_pixel) {
        *plane_pixel = PlanePixelValue(x, y, plane_index, frame_index);
    };
    ForEveryPlanePixel(width, height, frame_info, process, frame_data);
}

template<typename FrameInfo, typename FrameData>
static void CheckFrameContents(uint16_t width, uint16_t height, int frame_index,
    const FrameInfo& frame_info, const FrameData& frame_data)
{
    int fails_count = 0;

    ProcessPlanePixel process = [frame_index, &fails_count] (uint16_t x, uint16_t y, uint32_t plane_index, uint8_t* plane_pixel) {
        if (fails_count < 10) { // to not overflood output
            bool match = (*plane_pixel == PlanePixelValue(x, y, plane_index, frame_index));
            if (!match) ++fails_count;
            EXPECT_TRUE(match) << NAMED(x) << NAMED(y) << NAMED(plane_index);
        }
    };
    ForEveryPlanePixel(width, height, frame_info, process, frame_data);
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

#ifdef LIBVA_SUPPORT

static void InitNV12(uint16_t width, uint16_t height, mfxFrameInfo* frame_info)
{
    *frame_info = mfxFrameInfo {};
    frame_info->BitDepthLuma = 8;
    frame_info->BitDepthChroma = 8;
    frame_info->FourCC = MFX_FOURCC_NV12;
    frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    frame_info->Width = width;
    frame_info->Height = height;
    frame_info->CropX = 0;
    frame_info->CropY = 0;
    frame_info->CropW = width;
    frame_info->CropH = height;
    frame_info->FrameRateExtN = 30;
    frame_info->FrameRateExtD = 1;
    frame_info->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
}

struct MfxAllocTestRun {
    int width;
    int height;
    int frame_count;
};

typedef std::function<void (const MfxAllocTestRun& run, MfxFrameAllocator* allocator,
    mfxFrameAllocRequest& request, mfxFrameAllocResponse& response)> MfxVaAllocatorTestStep;

static void MfxVaAllocatorTest(const std::vector<MfxVaAllocatorTestStep>& steps)
{
    std::unique_ptr<MfxDev> dev { new MfxDevVa() };

    mfxStatus sts = dev->Init();
    EXPECT_EQ(MFX_ERR_NONE, sts);

    MfxFrameAllocator* allocator = dev->GetFrameAllocator();
    EXPECT_NE(allocator, nullptr);

    if (allocator) {

        MfxAllocTestRun test_allocations[] {
            { 600, 400, 3 },
            { 320, 240, 2 },
            { 1920, 1080, 3 },
        };

        mfxFrameAllocResponse responses[MFX_GET_ARRAY_SIZE(test_allocations)] {};
        mfxFrameAllocRequest requests[MFX_GET_ARRAY_SIZE(test_allocations)] {};

        for (auto& step : steps) {
            for (const MfxAllocTestRun& run : test_allocations) {
                mfxFrameAllocResponse& response = responses[&run - test_allocations];
                mfxFrameAllocRequest& request = requests[&run - test_allocations];

                step(run, allocator, request, response);
            }
        }
    }
    dev->Close();
}

static void MfxFrameAlloc(const MfxAllocTestRun& run, MfxFrameAllocator* allocator,
    mfxFrameAllocRequest& request, mfxFrameAllocResponse& response)
{
    request.Type = MFX_MEMTYPE_FROM_ENCODE;
    request.NumFrameMin = run.frame_count;
    request.NumFrameSuggested = run.frame_count;
    InitNV12(run.width, run.height, &request.Info);


    mfxStatus sts = allocator->AllocFrames(&request, &response);
    EXPECT_EQ(sts, MFX_ERR_NONE);
    EXPECT_EQ(response.NumFrameActual, request.NumFrameMin);

    EXPECT_NE(response.mids, nullptr);
}

static void MfxFrameFree(const MfxAllocTestRun&, MfxFrameAllocator* allocator,
    mfxFrameAllocRequest&, mfxFrameAllocResponse& response)
{
    mfxStatus sts = allocator->FreeFrames(&response);
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

// Tests mfxFrameAllocator implementation on VA.
// Checks Alloc and Free don't return any errors.
// Repeated many times to check possible memory leaks.
TEST(MfxVaAllocator, AllocFreeNoLeaks)
{
    const int COUNT = 1000;
    for (int i = 0; i < COUNT; ++i) {
        MfxVaAllocatorTest( { MfxFrameAlloc, MfxFrameFree } );
    }
}

// Tests mfxFrameAllocator implementation on VA.
// Executes GetFrameHDL on every allocated mem_id and assures all returned handles are different.
TEST(MfxVaAllocator, GetFrameHDL)
{
    std::set<mfxHDL> all_frame_handles;
    auto get_frame_hdl_test = [&all_frame_handles] (const MfxAllocTestRun& run, MfxFrameAllocator* allocator,
        mfxFrameAllocRequest&, mfxFrameAllocResponse& response) {

        EXPECT_NE(response.mids, nullptr);
        if (response.mids) {
            for (int i = 0; i < run.frame_count; ++i) {
                EXPECT_NE(response.mids[i], nullptr);

                mfxHDL frame_handle {};
                mfxStatus sts = allocator->GetFrameHDL(response.mids[i], &frame_handle);
                EXPECT_EQ(MFX_ERR_NONE, sts);
                EXPECT_NE(frame_handle, nullptr);

                EXPECT_EQ(all_frame_handles.find(frame_handle), all_frame_handles.end()); // test uniqueness
                all_frame_handles.insert(frame_handle);
            }
        }
    };

    MfxVaAllocatorTest( { MfxFrameAlloc, get_frame_hdl_test, MfxFrameFree } );
}

// Tests mfxFrameAllocator implementation on VA.
// The allocated buffer is locked, memory filled with some pattern,
// unlocked, then locked again, memory pattern should the same.
TEST(MfxVaAllocator, BufferKeepsContents)
{
    const bool hw_memory = true;
    const bool locked = true;

    auto lock_frame = [] (const MfxAllocTestRun& run, MfxFrameAllocator* allocator,
        mfxFrameAllocRequest& request, mfxFrameAllocResponse& response) {

        for (int i = 0; i < run.frame_count; ++i) {
            mfxFrameData frame_data {};
            mfxStatus sts = allocator->LockFrame(response.mids[i], &frame_data);
            EXPECT_EQ(MFX_ERR_NONE, sts);

            CheckNV12MfxFrameData(run.width, run.height, hw_memory, locked, frame_data);

            FillFrameContents(run.width, run.height, i, request.Info, frame_data);

            sts = allocator->UnlockFrame(response.mids[i], &frame_data);
            EXPECT_EQ(MFX_ERR_NONE, sts);
        }
    };

    auto unlock_frame = [] (const MfxAllocTestRun& run, MfxFrameAllocator* allocator,
        mfxFrameAllocRequest& request, mfxFrameAllocResponse& response) {

        for (int i = 0; i < run.frame_count; ++i) {
            mfxFrameData frame_data {};
            mfxStatus sts = allocator->LockFrame(response.mids[i], &frame_data);
            EXPECT_EQ(MFX_ERR_NONE, sts);

            CheckNV12MfxFrameData(run.width, run.height, hw_memory, locked, frame_data);

            CheckFrameContents(run.width, run.height, i, request.Info, frame_data);

            sts = allocator->UnlockFrame(response.mids[i], &frame_data);
            EXPECT_EQ(MFX_ERR_NONE, sts);
        }
    };

    MfxVaAllocatorTest( { MfxFrameAlloc, lock_frame, unlock_frame, MfxFrameFree } );
}

#endif
