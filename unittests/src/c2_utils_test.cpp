/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <gtest/gtest.h>
#include "mfx_cmd_queue.h"
#include "mfx_pool.h"
#include "mfx_gralloc_allocator.h"
#include "mfx_va_allocator.h"
#include "mfx_frame_pool_allocator.h"
#include "C2PlatformSupport.h"
#include "mfx_c2_utils.h"
#include <map>
#include <set>
#include "test_streams.h"
#include "streams/h264/stream_nv12_176x144_cqp_g30_100.264.h"
#include "streams/h264/stream_nv12_352x288_cqp_g15_100.264.h"
#include "streams/h265/stream_nv12_176x144_cqp_g30_100.265.h"
#include "streams/h265/stream_nv12_352x288_cqp_g15_100.265.h"
#include "streams/vp9/stream_nv12_176x144_cqp_g30_100.vp9.ivf.h"
#include "streams/vp9/stream_nv12_352x288_cqp_g15_100.vp9.ivf.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"

#ifdef LIBVA_SUPPORT
#include "mfx_dev_va.h"
#include <va/va_android.h>
#else
#include "mfx_dev_android.h"
#endif

using namespace android;

static const size_t CMD_COUNT = 10;

#define MOCK_COMPONENT_ENC "c2.intel.mock.encoder"
#define MOCK_COMPONENT MOCK_COMPONENT_ENC // use encoder for common tests

static std::vector<const StreamDescription*> g_streams {
    &stream_nv12_176x144_cqp_g30_100_264,
    &stream_nv12_352x288_cqp_g15_100_264,
    &stream_nv12_176x144_cqp_g30_100_265,
    &stream_nv12_352x288_cqp_g15_100_265,
    &stream_nv12_176x144_cqp_g30_100_vp9_ivf,
    &stream_nv12_352x288_cqp_g15_100_vp9_ivf
};

// Multiple streams read till the end with CombinedStreamReader should give the same output
// as those streams read with SingleStreamReader instances.
TEST(CombinedStreamReader, Read)
{
    bool header {};
    StreamDescription::Region region;

    std::list<std::vector<char>> single_readers_res;
    {
        std::vector<SingleStreamReader> readers;
        for (const auto& stream : g_streams ) {
            readers.emplace_back(stream);
        }

        for (auto& reader : readers) {
            while (reader.Read(StreamReader::Slicing::Frame(), &region, &header)) {
                single_readers_res.push_back(reader.GetRegionContents(region));
            }
        }
    }

    std::list<std::vector<char>> combined_reader_res;
    {
        CombinedStreamReader reader(g_streams);

        while (reader.Read(StreamReader::Slicing::Frame(), &region, &header)) {
            combined_reader_res.push_back(reader.GetRegionContents(region));
        }
    }

    EXPECT_EQ(single_readers_res, combined_reader_res);
}

// Reads from stream by one byte, EndOfStream should give true iff next Read is imposssible.
TEST(CombinedStreamReader, EndOfStream)
{
    CombinedStreamReader reader(g_streams);

    bool header {};
    StreamDescription::Region region;

    for (;;) {
        bool eos = reader.EndOfStream();
        bool read_ok = reader.Read(StreamReader::Slicing(1), &region, &header);
        EXPECT_NE(read_ok, eos);
        if (!read_ok) break;
    }
}

// Seek to position around edge between adjacent streams, read some chunk of data
// and compares it with part in all contents array.
TEST(CombinedStreamReader, Seek)
{
    size_t total_size = 0;
    for (const auto& stream : g_streams) {
        total_size += stream->data.size();
    }

    bool header {};
    StreamDescription::Region region;

    std::vector<char> combined_reader_res;
    CombinedStreamReader reader(g_streams);

    while (reader.Read(StreamReader::Slicing(1024), &region, &header)) {
        std::vector<char> chunk = reader.GetRegionContents(region);
        combined_reader_res.insert(combined_reader_res.end(),
            chunk.begin(), chunk.end());
    }
    EXPECT_EQ(combined_reader_res.size(), total_size); // check read all contents

    const size_t len = 100; // len to read
    size_t edge = 0;
    for (size_t i = 0; i < g_streams.size() - 1; ++i) {
        edge += g_streams[i]->data.size();

        for (size_t start : { edge - len, edge - len / 2, edge, edge + len, edge - len } ) {
            reader.Seek(start);
            bool res = reader.Read(StreamReader::Slicing(len), &region, &header);
            EXPECT_TRUE(res);
            std::vector<char> chunk = reader.GetRegionContents(region);

            auto mismatch_res = std::mismatch(chunk.begin(), chunk.end(),
                combined_reader_res.begin() + start);
            EXPECT_EQ(mismatch_res.first, chunk.end());
        }
    }
}

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

// Tests command queue doesn't crash on invalid Start/Stop sequences.
TEST(MfxCmdQueue, StartStop)
{
    {   // double start
        MfxCmdQueue queue;
        queue.Start();
        queue.Start();
    }
    {   // double stop
        MfxCmdQueue queue;
        queue.Start();
        queue.Stop();
        queue.Stop();
    }
    {   // stop not started
        MfxCmdQueue queue;
        queue.Stop();
    }
    {   // destruct running queue
        MfxCmdQueue queue;
        queue.Start();
    }
}

// Tests abstract command queue handles Pause/Resume fine.
TEST(MfxCmdQueue, PauseResume)
{
    MfxCmdQueue queue;
    queue.Start();

    std::vector<size_t> result;
    const size_t PAUSE_CMD_INDEX = CMD_COUNT / 2;

    for(size_t i = 0; i < CMD_COUNT; ++i) {

        std::unique_ptr<int> ptr_i = std::make_unique<int>(i);

        // lambda mutable and not copy-assignable to assert MfxCmdQueue supports it
        auto task = [ ptr_i = std::move(ptr_i), &queue, &result] () mutable {
            result.push_back(*ptr_i);
            if (*ptr_i == PAUSE_CMD_INDEX) {
                queue.Pause();
            }
            ptr_i = nullptr;
        };

        queue.Push(std::move(task));
    }
    // Wait long enough to run into task with queue.Pause.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    // Expect tasks processed till PAUSE_CMD_INDEX including.
    EXPECT_EQ(result.size(), PAUSE_CMD_INDEX + 1);
    for(size_t i = 0; i < result.size(); ++i) {
        EXPECT_EQ(result[i], i);
    }

    queue.Resume();
    queue.Stop();
    // Resume and Stop above should complete tasks remained.
    EXPECT_EQ(result.size(), CMD_COUNT);
    for(size_t i = PAUSE_CMD_INDEX + 1; i < result.size(); ++i) {
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

// Tests that MfxPool allocates values among appended
// and if no resources available, correctly waits for freeing resources.
// Also checks allocated values are valid after pool destruction.
TEST(MfxPool, Alloc)
{
    const int COUNT = 10;
    std::shared_ptr<int> allocated_again[COUNT];

    {
        MfxPool<int> pool;
        // append range of numbers
        for (int i = 0; i < COUNT; ++i) {
            pool.Append(std::make_shared<int>(i));
        }

        std::shared_ptr<int> allocated[COUNT];
        for (int i = 0; i < COUNT; ++i) {
            allocated[i] = pool.Alloc();
            EXPECT_EQ(*allocated[i], i); // check values are those appended
        }

        std::thread free_thread([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            for (int i = 0; i < COUNT; ++i) {
                allocated[i].reset();
            }
        });

        auto start = std::chrono::system_clock::now();
        for (int i = 0; i < COUNT; ++i) {
            allocated_again[i] = pool.Alloc(); // this Alloc should wait for free in free_thread
            EXPECT_EQ(*allocated_again[i], i); // and got the same value
        }
        auto end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;
        // elapsed time is around 0.5 seconds when free_thread deallocates the resources
        EXPECT_TRUE((0.4 < elapsed_seconds.count()) && (elapsed_seconds.count() < 0.6));

        free_thread.join();

        start = std::chrono::system_clock::now();
        std::shared_ptr<int> allocation_failed = pool.Alloc();
        EXPECT_EQ(allocation_failed, nullptr); // should get null in timeout value (1 second)
        end = std::chrono::system_clock::now();
        elapsed_seconds = end - start;
        EXPECT_TRUE((0.9 < elapsed_seconds.count()) && (elapsed_seconds.count() < 1.1));
    }
    // check allocated_again have correct values after pool destruction
    for (int i = 0; i < COUNT; ++i) {
        EXPECT_EQ(*allocated_again[i], i);
    }
}

// Tests MfxDev could be created and released significant amount of times.
// For pure build this tests MfxDevAndroid, for VA - MfxDevVa.
TEST(MfxDev, InitCloseNoLeaks)
{
    const int COUNT = 1500;

    for (int i = 0; i < COUNT; ++i)
    {
        std::unique_ptr<MfxDev> device;
        mfxStatus sts = MfxDev::Create(MfxDev::Usage::Decoder, &device);

        EXPECT_EQ(MFX_ERR_NONE, sts);
        EXPECT_NE(device, nullptr);

        sts = device->Close();
        EXPECT_EQ(MFX_ERR_NONE, sts);
    }
}

static void CheckNV12PlaneLayout(uint16_t width, uint16_t height, const C2PlanarLayout& layout,
    const uint8_t* const* data)
{
    using Layout = C2PlanarLayout;
    using Info = C2PlaneInfo;

    EXPECT_EQ(layout.type, Layout::TYPE_YUV);
    EXPECT_EQ(layout.numPlanes, 3u);
    EXPECT_EQ(layout.rootPlanes, 2u);

    std::map<Layout::plane_index_t, Info::channel_t> expected_channels = {
        {  Layout::PLANE_Y, Info::CHANNEL_Y },
        {  Layout::PLANE_U, Info::CHANNEL_CB },
        {  Layout::PLANE_V, Info::CHANNEL_CR },
    };

    for (Layout::plane_index_t index : { Layout::PLANE_Y, Layout::PLANE_U, Layout::PLANE_V }) {
        EXPECT_EQ(layout.planes[index].channel, expected_channels[index]);
        EXPECT_EQ(layout.planes[index].colInc, index == Layout::PLANE_Y ? 1 : 2);
        EXPECT_TRUE(layout.planes[index].rowInc >= width);
        EXPECT_EQ(layout.planes[index].colSampling, index == Layout::PLANE_Y ? 1u : 2u);
        EXPECT_EQ(layout.planes[index].rowSampling, index == Layout::PLANE_Y ? 1u : 2u);
        EXPECT_EQ(layout.planes[index].bitDepth, 8u);
        EXPECT_EQ(layout.planes[index].allocatedDepth, 8u);
        EXPECT_EQ(layout.planes[index].rightShift, 0u);
        EXPECT_EQ(layout.planes[index].endianness, C2PlaneInfo::NATIVE);
        EXPECT_EQ(layout.planes[index].rootIx, index == Layout::PLANE_Y ? Layout::PLANE_Y : Layout::PLANE_U);
        EXPECT_EQ(layout.planes[index].offset, index != Layout::PLANE_V ? 0u : 1u);

        EXPECT_NE(data[index], nullptr);
        if (index != Layout::PLANE_Y) {
            EXPECT_TRUE(data[index] - data[0] >= layout.planes[Layout::PLANE_Y].rowInc * height);
        }
    }
    EXPECT_EQ(data[Layout::PLANE_U] + 1, data[Layout::PLANE_V]);
}

#ifdef LIBVA_SUPPORT

static void CheckMfxFrameData(mfxU32 fourcc, uint16_t width, uint16_t height,
    bool hw_memory, bool locked, const mfxFrameData& frame_data)
{
    EXPECT_EQ(frame_data.PitchHigh, 0);
    uint32_t pitch = MakeUint32(frame_data.PitchHigh, frame_data.PitchLow);

    if (fourcc != MFX_FOURCC_P8) {
        EXPECT_TRUE(pitch >= width);
    }
    EXPECT_EQ(frame_data.MemId != nullptr, hw_memory);

    bool pointers_expected = locked || !hw_memory;
    bool color = (fourcc != MFX_FOURCC_P8);

    EXPECT_EQ(pointers_expected, frame_data.Y != nullptr);
    EXPECT_EQ(pointers_expected && color, frame_data.UV != nullptr);
    EXPECT_EQ(pointers_expected && color, frame_data.V != nullptr);

    if(pointers_expected && color) {
        EXPECT_TRUE(frame_data.Y + pitch * height <= frame_data.UV);
        EXPECT_EQ(frame_data.UV + 1, frame_data.V);
    }
    EXPECT_EQ(frame_data.A, nullptr);
}

#endif

static uint8_t PlanePixelValue(uint16_t x, uint16_t y, uint32_t plane_index, int frame_index)
{
    return (uint8_t)(x + y + plane_index + frame_index);
}

typedef std::function<void(uint16_t x, uint16_t y, uint32_t plane_index, uint8_t* plane_pixel)> ProcessPlanePixel;

static void ForEveryPlanePixel(uint16_t width, uint16_t height, const C2PlanarLayout& layout,
    const ProcessPlanePixel& process_function, uint8_t* const* data)
{
    for (uint32_t i = 0; i < layout.numPlanes; ++i) {
        const C2PlaneInfo& plane = layout.planes[i];

        uint8_t* row = data[i];
        for (uint16_t y = 0; y < height; y += plane.rowSampling) {
            uint8_t* pixel = row;
            for (uint16_t x = 0; x < width; x += plane.colSampling) {
                process_function(x, y, i, pixel);
                pixel += plane.colInc;
            }
            row += plane.rowInc;
        }
    }
}

#ifdef LIBVA_SUPPORT

static void ForEveryPlanePixel(uint16_t width, uint16_t height, const mfxFrameInfo& frame_info,
    const ProcessPlanePixel& process_function, const mfxFrameData& frame_data)
{
    const int planes_count_max = 3;
    uint8_t* planes_data[planes_count_max] = { frame_data.Y, frame_data.UV, frame_data.UV + 1 };
    const uint16_t planes_vert_subsampling[planes_count_max] = { 1, 2, 2 };
    const uint16_t planes_horz_subsampling[planes_count_max] = { 1, 2, 2 };
    const uint16_t planes_col_inc[planes_count_max] = { 1, 2, 2 };

    int planes_count = -1;

    switch (frame_info.FourCC) {
        case MFX_FOURCC_NV12:
            EXPECT_EQ(frame_info.ChromaFormat, MFX_CHROMAFORMAT_YUV420);
            planes_count = 3;
            break;
        case MFX_FOURCC_P8:
            EXPECT_EQ(frame_info.ChromaFormat, MFX_CHROMAFORMAT_MONOCHROME);
            planes_count = 1;
            // buffer is linear, set up width and height to one line
            width = EstimatedEncodedFrameLen(width, height);
            height = 1;
            break;
        default:
            EXPECT_TRUE(false) << "unsupported color format";
    }

    uint32_t pitch = MakeUint32(frame_data.PitchHigh, frame_data.PitchLow);

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

#endif

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
            uint8_t actual = *plane_pixel;
            uint8_t expected = PlanePixelValue(x, y, plane_index, frame_index);
            bool match = (actual == expected);
            if (!match) ++fails_count;
            EXPECT_TRUE(match) << NAMED(x) << NAMED(y) << NAMED(plane_index)
                << NAMED((int)actual) << NAMED((int)expected);
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

    c2_status_t res = MfxGrallocAllocator::Create(&allocator);
    EXPECT_EQ(res, C2_OK);
    EXPECT_NE(allocator, nullptr);

    const int WIDTH = 600;
    const int HEIGHT = 400;
    const size_t FRAME_COUNT = 3;

    if (allocator) {

        buffer_handle_t handle[FRAME_COUNT] {};
        c2_status_t res;

        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            res = allocator->Alloc(WIDTH, HEIGHT, &handle[i]);
            EXPECT_EQ(res, C2_OK);
            EXPECT_NE(handle, nullptr);
        }

        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            uint8_t* data[C2PlanarLayout::MAX_NUM_PLANES] {};
            C2PlanarLayout layout {};
            res = allocator->LockFrame(handle[i], data, &layout);
            EXPECT_EQ(res, C2_OK);

            CheckNV12PlaneLayout(WIDTH, HEIGHT, layout, data);

            FillFrameContents(WIDTH, HEIGHT, i, layout, data);

            res = allocator->UnlockFrame(handle[i]);
            EXPECT_EQ(res, C2_OK);
        }

        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            uint8_t* data[C2PlanarLayout::MAX_NUM_PLANES] {};
            C2PlanarLayout layout {};
            res = allocator->LockFrame(handle[i], data, &layout);
            EXPECT_EQ(res, C2_OK);

            CheckNV12PlaneLayout(WIDTH, HEIGHT, layout, data);

            CheckFrameContents(WIDTH, HEIGHT, i, layout, data);

            res = allocator->UnlockFrame(handle[i]);
            EXPECT_EQ(res, C2_OK);
        }

        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            res = allocator->Free(handle[i]);
            EXPECT_EQ(res, C2_OK);
        }
    }
}

#ifdef LIBVA_SUPPORT

static void InitFrameInfo(mfxU32 fourcc, uint16_t width, uint16_t height, mfxFrameInfo* frame_info)
{
    *frame_info = mfxFrameInfo {};
    frame_info->BitDepthLuma = 8;
    frame_info->BitDepthChroma = 8;
    frame_info->FourCC = fourcc;

    switch (fourcc) {
        case MFX_FOURCC_NV12:
            frame_info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
            break;
        case MFX_FOURCC_P8:
            frame_info->ChromaFormat = MFX_CHROMAFORMAT_MONOCHROME;
            break;
        default:
            ASSERT_TRUE(false) << std::hex << fourcc << " format is not supported";
    }

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

class UtilsVaContext
{
private:
    VAConfigID va_config_ { VA_INVALID_ID };

    VAContextID va_context_ { VA_INVALID_ID };

    VADisplay va_display_ { nullptr };

public:
    UtilsVaContext(VADisplay va_display, int width, int height)
        : va_display_(va_display)
    {
        VAConfigAttrib attrib[2];
        mfxI32 numAttrib = MFX_GET_ARRAY_SIZE(attrib);
        attrib[0].type = VAConfigAttribRTFormat;
        attrib[0].value = VA_RT_FORMAT_YUV420;
        attrib[1].type = VAConfigAttribRateControl;
        attrib[1].value = VA_RC_CQP;

        mfxU32 flag = VA_PROGRESSIVE;

        VAProfile va_profile = VAProfileH264ConstrainedBaseline;
        VAEntrypoint entrypoint = VAEntrypointEncSlice;
        VAStatus sts = vaCreateConfig(va_display_, va_profile, entrypoint, attrib, numAttrib, &va_config_);
        EXPECT_EQ(sts, VA_STATUS_SUCCESS);
        EXPECT_NE(va_config_, VA_INVALID_ID);

        if (VA_INVALID_ID != va_config_) {
            sts = vaCreateContext(va_display_, va_config_, width, height, flag, nullptr, 0, &va_context_);
            EXPECT_EQ(sts, VA_STATUS_SUCCESS);
            EXPECT_NE(va_context_, VA_INVALID_ID);
        }
    }

    ~UtilsVaContext()
    {
        if (va_config_ != VA_INVALID_ID) vaDestroyConfig(va_display_, va_config_);
        if (va_context_ != VA_INVALID_ID) vaDestroyContext(va_display_, va_context_);
    }

    VAContextID GetVaContext() { return va_context_; }
};

struct MfxAllocTestRun {
    int width;
    int height;
    int frame_count;
    mfxU32 fourcc;
};

typedef std::function<void (const MfxAllocTestRun& run, MfxFrameAllocator* allocator,
    mfxFrameAllocRequest& request, mfxFrameAllocResponse& response)> MfxVaAllocatorTestStep;

static void MfxVaAllocatorTest(const std::vector<MfxVaAllocatorTestStep>& steps, int repeat_count = 1)
{
    MfxDevVa* dev_va = new MfxDevVa(MfxDev::Usage::Encoder);
    std::unique_ptr<MfxDev> dev { dev_va };

    mfxStatus sts = dev->Init();
    EXPECT_EQ(MFX_ERR_NONE, sts);

    std::shared_ptr<MfxFrameAllocator> allocator = dev->GetFrameAllocator();
    EXPECT_NE(allocator, nullptr);

    if (allocator) {

        MfxAllocTestRun test_allocations[] {
            { 600, 400, 3, MFX_FOURCC_NV12 },
            { 320, 240, 2, MFX_FOURCC_NV12 },
            { 1920, 1080, 3, MFX_FOURCC_NV12 },
            { 1280, 720, 3, MFX_FOURCC_P8 },
        };

        mfxFrameAllocResponse responses[MFX_GET_ARRAY_SIZE(test_allocations)] {};
        mfxFrameAllocRequest requests[MFX_GET_ARRAY_SIZE(test_allocations)] {};
        std::unique_ptr<UtilsVaContext> va_contexts[MFX_GET_ARRAY_SIZE(test_allocations)];

        for (MfxAllocTestRun& run : test_allocations) {
            if (run.fourcc == MFX_FOURCC_P8) {
                va_contexts[&run - test_allocations] =
                    std::make_unique<UtilsVaContext>(dev_va->GetVaDisplay(), run.width, run.height);
            }
        }

        for (int i = 0; i < repeat_count; ++i) {
            for (auto& step : steps) {
                for (const MfxAllocTestRun& run : test_allocations) {
                    const int index = &run - test_allocations;
                    mfxFrameAllocResponse& response = responses[index];
                    mfxFrameAllocRequest& request = requests[index];

                    if (va_contexts[index]) {

                        if (va_contexts[index]->GetVaContext() == VA_INVALID_ID) continue;

                        request.AllocId = va_contexts[index]->GetVaContext();
                    }

                    step(run, allocator.get(), request, response);
                }
            }
        }

        allocator.reset();
    }
    sts = dev->Close();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

static void MfxFrameAlloc(const MfxAllocTestRun& run, MfxFrameAllocator* allocator,
    mfxFrameAllocRequest& request, mfxFrameAllocResponse& response)
{
    request.Type = MFX_MEMTYPE_FROM_ENCODE;
    request.NumFrameMin = run.frame_count;
    request.NumFrameSuggested = run.frame_count;
    InitFrameInfo(run.fourcc, run.width, run.height, &request.Info);

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
    MfxVaAllocatorTest( { MfxFrameAlloc, MfxFrameFree }, COUNT );
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

            CheckMfxFrameData(run.fourcc, run.width, run.height, hw_memory, locked, frame_data);

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

            CheckMfxFrameData(run.fourcc, run.width, run.height, hw_memory, locked, frame_data);

            CheckFrameContents(run.width, run.height, i, request.Info, frame_data);

            sts = allocator->UnlockFrame(response.mids[i], &frame_data);
            EXPECT_EQ(MFX_ERR_NONE, sts);
        }
    };

    MfxVaAllocatorTest( { MfxFrameAlloc, lock_frame, unlock_frame, MfxFrameFree } );
}

typedef std::function<void (MfxGrallocAllocator* gr_allocator, MfxFrameAllocator* allocator,
    MfxFrameConverter* converter)> MfxFrameConverterTestStep;

static void MfxFrameConverterTest(const std::vector<MfxFrameConverterTestStep>& steps, int repeat_count = 1)
{
    std::unique_ptr<MfxGrallocAllocator> gr_allocator;

    c2_status_t res = MfxGrallocAllocator::Create(&gr_allocator);
    EXPECT_EQ(res, C2_OK);
    EXPECT_NE(gr_allocator, nullptr);

    std::unique_ptr<MfxDev> dev { new MfxDevVa(MfxDev::Usage::Encoder) };

    mfxStatus sts = dev->Init();
    EXPECT_EQ(MFX_ERR_NONE, sts);

    std::shared_ptr<MfxFrameAllocator> allocator = dev->GetFrameAllocator();
    EXPECT_NE(allocator, nullptr);

    std::shared_ptr<MfxFrameConverter> converter = dev->GetFrameConverter();
    EXPECT_NE(converter, nullptr);

    if (gr_allocator && allocator && converter) {
        for (int i = 0; i < repeat_count; ++i) {
            for (auto& step : steps) {
                step(gr_allocator.get(), allocator.get(), converter.get());
            }
        }
    }

    converter->FreeAllMappings();
    converter.reset();
    allocator.reset();

    sts = dev->Close();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

// Class implementing variety of steps for MfxFrameConverter tests.
class MfxFrameConverterTestSteps
{
public:
    using Step = MfxFrameConverterTestStep;
    // operation on frame mapped from gralloc to system memory
    typedef std::function<void (int frame_index, const C2PlanarLayout& layout, uint8_t* const* data)> GrMemOperation;
    // operation on frame mapped from va to system memory
    typedef std::function<void (int frame_index,
        const mfxFrameInfo& frame_info, mfxFrameData& frame_data)> VaMemOperation;

public:
    // gralloc allocation step
    Step gr_alloc = [this] (MfxGrallocAllocator* gr_allocator, MfxFrameAllocator*, MfxFrameConverter*) {
        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            c2_status_t res = gr_allocator->Alloc(WIDTH, HEIGHT, &handles[i]);
            EXPECT_EQ(res, C2_OK);
            EXPECT_NE(handles[i], nullptr);
        }
    };

    // c2 graphic allocation step
    Step c2_alloc = [this] (MfxGrallocAllocator*, MfxFrameAllocator*, MfxFrameConverter*) {

        std::shared_ptr<C2BlockPool> c2_allocator;
        c2_status_t res = C2_OK;

        std::shared_ptr<MfxC2ParamReflector> reflector = std::make_shared<MfxC2ParamReflector>();
        std::shared_ptr<const C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, {}, reflector, &res));
        EXPECT_EQ(res, C2_OK);

        res = GetCodec2BlockPool(C2BlockPool::BASIC_GRAPHIC, component, &c2_allocator);
        EXPECT_EQ(res, C2_OK);
        EXPECT_NE(c2_allocator, nullptr);

        if (c2_allocator) {
            for (size_t i = 0; i < FRAME_COUNT; ++i) {
                res = c2_allocator->fetchGraphicBlock(
                    WIDTH, HEIGHT,
                    HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,
                    // HW_CODEC forces VNDK mock to allocate gralloc memory, not system
                    { android::C2AndroidMemoryUsage::HW_CODEC_READ, android::C2AndroidMemoryUsage::HW_CODEC_WRITE },
                    &gr_blocks[i]);

                handles[i] = gr_blocks[i]->handle();

                EXPECT_EQ(res, C2_OK);
                EXPECT_NE(handles[i], nullptr);
            }
        }
    };

    // gralloc deallocation step
    Step gr_free = [this] (MfxGrallocAllocator* gr_allocator, MfxFrameAllocator*, MfxFrameConverter*) {

        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            c2_status_t res = gr_allocator->Free(handles[i]);
            EXPECT_EQ(res, C2_OK);
        }
    };

    // c2 deallocation step
    Step c2_free = [this] (MfxGrallocAllocator*, MfxFrameAllocator*, MfxFrameConverter*) {

        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            gr_blocks[i] = nullptr;
        }
    };

    // lambda constructing test step doing: gralloc Lock, some specific work on locked memory, gralloc unlock
    Step do_gr_mem_operation(GrMemOperation gr_mem_operation) {
        return [&] (MfxGrallocAllocator* gr_allocator, MfxFrameAllocator*, MfxFrameConverter*) {

            for (size_t i = 0; i < FRAME_COUNT; ++i) {
                uint8_t* data[C2PlanarLayout::MAX_NUM_PLANES] {};
                C2PlanarLayout layout {};
                c2_status_t res = gr_allocator->LockFrame(handles[i], data, &layout);
                EXPECT_EQ(res, C2_OK);
                if (C2_OK == res) {
                    CheckNV12PlaneLayout(WIDTH, HEIGHT, layout, data);

                    gr_mem_operation(i, layout, data);

                    res = gr_allocator->UnlockFrame(handles[i]);
                    EXPECT_EQ(res, C2_OK);
                }
            };
        };
    };

    // lambda constructing test step doing: c2 Lock, some specific work on locked memory, c2 unlock
    Step do_c2_mem_operation(GrMemOperation gr_mem_operation) {
        return [&] (MfxGrallocAllocator*, MfxFrameAllocator*, MfxFrameConverter*) {

            for (size_t i = 0; i < FRAME_COUNT; ++i) {
                C2Acquirable<C2GraphicView> acquirable = gr_blocks[i]->map();
                C2GraphicView view = acquirable.get();
                EXPECT_EQ(view.error(), C2_OK);
                if (C2_OK == view.error()) {
                    CheckNV12PlaneLayout(WIDTH, HEIGHT, view.layout(), view.data());

                    gr_mem_operation(i, view.layout(), view.data());
                }
            };
        };
    };

    // gralloc to va wiring step
    Step gr_convert_to_va = [this] (MfxGrallocAllocator*, MfxFrameAllocator*, MfxFrameConverter* converter) {

        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            bool decode_target { false };
            mfxStatus mfx_sts = converter->ConvertGrallocToVa(handles[i], decode_target, &mfx_mem_ids[i]);
            EXPECT_EQ(MFX_ERR_NONE, mfx_sts);
            EXPECT_NE(mfx_mem_ids[i], nullptr);
        }
    };

    // lambda constructing test step doing: va Lock, some specific work on locked memory, va unlock
    Step do_va_mem_operation(VaMemOperation va_mem_operation) {
        return [&] (MfxGrallocAllocator*, MfxFrameAllocator* allocator, MfxFrameConverter*) {

            const bool hw_memory = true;
            const bool locked = true;

            mfxFrameInfo frame_info {};
            InitFrameInfo(MFX_FOURCC_NV12, WIDTH, HEIGHT, &frame_info);

            for (size_t i = 0; i < FRAME_COUNT; ++i) {
                mfxFrameData frame_data {};
                mfxStatus sts = allocator->LockFrame(mfx_mem_ids[i], &frame_data);
                EXPECT_EQ(MFX_ERR_NONE, sts);
                if (MFX_ERR_NONE == sts) {
                    CheckMfxFrameData(MFX_FOURCC_NV12, WIDTH, HEIGHT, hw_memory, locked, frame_data);

                    va_mem_operation(i, frame_info, frame_data);

                    sts = allocator->UnlockFrame(mfx_mem_ids[i], &frame_data);
                    EXPECT_EQ(MFX_ERR_NONE, sts);
                }
            }
        };
    };

    GrMemOperation fill_gr_frame = [this] (int frame_index, const C2PlanarLayout& layout, uint8_t* const* data) {
        FillFrameContents(WIDTH, HEIGHT, frame_index, layout, data); // fill gralloc with pattern #1
    };

    VaMemOperation check_va_frame = [this] (int frame_index, const mfxFrameInfo& frame_info, mfxFrameData& frame_data) {
        CheckFrameContents(WIDTH, HEIGHT, frame_index, frame_info, frame_data); // check pattern #1 in va
    };

    VaMemOperation fill_va_frame = [this] (int frame_index, const mfxFrameInfo& frame_info, mfxFrameData& frame_data) {
        // fill va with pattern #2
        FillFrameContents(WIDTH, HEIGHT, FRAME_COUNT - frame_index, frame_info, frame_data);
    };

    GrMemOperation check_gr_frame = [this] (int frame_index, const C2PlanarLayout& layout, uint8_t* const* data) {
        // check pattern #2 in gralloc
        CheckFrameContents(WIDTH, HEIGHT, FRAME_COUNT - frame_index, layout, data);
    };

    Step free_mappings = [] (MfxGrallocAllocator*, MfxFrameAllocator*, MfxFrameConverter* converter) {
        converter->FreeAllMappings();
    };

private:
    const int WIDTH = 600;
    const int HEIGHT = 400;
    static constexpr int FRAME_COUNT = 3;

private:
    std::shared_ptr<C2GraphicBlock> gr_blocks[FRAME_COUNT];
    buffer_handle_t handles[FRAME_COUNT] {};
    mfxMemId mfx_mem_ids[FRAME_COUNT] {};
};

// Allocates some gralloc frames with MfxGrallocAllocator,
// fills them with pattern,
// wires them up with mfxMemID (VA surface inside),
// locks mfxFrames and checks a pattern is the same.
// Then locks mfxFrames again, fills them with different pattern
// and checks original gralloc buffers get updated pattern.
// These steps prove modifications go from gralloc to VA and back.
TEST(MfxFrameConverter, GrallocContentsMappedToVa)
{
    MfxFrameConverterTestSteps steps;
    // test direct gralloc allocation wiring to va
    MfxFrameConverterTest( {
        steps.gr_alloc,
        steps.do_gr_mem_operation(steps.fill_gr_frame),
        steps.gr_convert_to_va,
        steps.do_va_mem_operation(steps.check_va_frame),
        steps.do_va_mem_operation(steps.fill_va_frame),
        steps.do_gr_mem_operation(steps.check_gr_frame),
        steps.free_mappings,
        steps.gr_free
    } );
}

// Allocates some C2GraphicBlock instances with C2 vndk GrallocAllocator,
// fills them with pattern,
// wires them up with mfxMemID (VA surface inside),
// locks mfxFrames and checks a pattern is the same.
// Then locks mfxFrames again, fills them with different pattern
// and checks original gralloc buffers get updated pattern.
// These steps prove modifications go from C2GraphicBlock to VA and back.
TEST(MfxFrameConverter, C2GraphicBlockContentsMappedToVa)
{
    MfxFrameConverterTestSteps steps;
    // test c2 vndk allocation wiring to va
    MfxFrameConverterTest( {
        steps.c2_alloc,
        steps.do_c2_mem_operation(steps.fill_gr_frame),
        steps.gr_convert_to_va,
        steps.do_va_mem_operation(steps.check_va_frame),
        steps.do_va_mem_operation(steps.fill_va_frame),
        steps.do_c2_mem_operation(steps.check_gr_frame),
        steps.free_mappings,
        steps.c2_free
    } );
}

// Allocates and maps gralloc handles to VA.
// Then frees resources in different ways, checks it works
// significant amount of times.
TEST(MfxFrameConverter, NoLeaks)
{
    const int WIDTH = 1920;
    const int HEIGHT = 1080;
    const int REPEAT_COUNT = 500;

    buffer_handle_t handle {};
    mfxMemId mfx_mem_id {};

    auto alloc_and_map = [&] (MfxGrallocAllocator* gr_allocator, MfxFrameAllocator*,
        MfxFrameConverter* converter) {

        c2_status_t res = gr_allocator->Alloc(WIDTH, HEIGHT, &handle);
        EXPECT_EQ(res, C2_OK);
        EXPECT_NE(handle, nullptr);

        bool decode_target { false };
        mfxStatus mfx_sts = converter->ConvertGrallocToVa(handle, decode_target, &mfx_mem_id);
        EXPECT_EQ(MFX_ERR_NONE, mfx_sts);
        EXPECT_NE(mfx_mem_id, nullptr);
    };

    auto gr_free = [&] (MfxGrallocAllocator* gr_allocator, MfxFrameAllocator*, MfxFrameConverter*) {
        c2_status_t res = gr_allocator->Free(handle);
        EXPECT_EQ(res, C2_OK);
    };

    auto free_all = [] (MfxGrallocAllocator*, MfxFrameAllocator*, MfxFrameConverter* converter) {
        converter->FreeAllMappings();
    };

    MfxFrameConverterTest( { alloc_and_map, free_all, gr_free }, REPEAT_COUNT );

    auto free_by_handles = [&] (MfxGrallocAllocator*, MfxFrameAllocator*,
        MfxFrameConverter* converter) {

        converter->FreeGrallocToVaMapping(handle);
    };

    MfxFrameConverterTest( { alloc_and_map, free_by_handles, gr_free }, REPEAT_COUNT );

    auto free_by_mids = [&] (MfxGrallocAllocator*, MfxFrameAllocator*,
        MfxFrameConverter* converter) {

        converter->FreeGrallocToVaMapping(mfx_mem_id);
    };

    MfxFrameConverterTest( { alloc_and_map, free_by_mids, gr_free }, REPEAT_COUNT );
}

// Checks converter returns the same mem_id for the same gralloc handle.
TEST(MfxFrameConverter, CacheResources)
{
    const int WIDTH = 1920;
    const int HEIGHT = 1080;
    const int REPEAT_COUNT = 10;

    auto test_cache = [&] (MfxGrallocAllocator* gr_allocator, MfxFrameAllocator*,
        MfxFrameConverter* converter) {

        buffer_handle_t handle {};

        c2_status_t res = gr_allocator->Alloc(WIDTH, HEIGHT, &handle);
        EXPECT_EQ(res, C2_OK);
        EXPECT_NE(handle, nullptr);

        mfxMemId mfx_mem_ids[REPEAT_COUNT] {};

        for (int i = 0; i < REPEAT_COUNT; ++i) {
            bool decode_target { false };
            mfxStatus mfx_sts = converter->ConvertGrallocToVa(handle, decode_target, &mfx_mem_ids[i]);
            EXPECT_EQ(MFX_ERR_NONE, mfx_sts);
            EXPECT_NE(mfx_mem_ids[i], nullptr);
        }

        ASSERT_TRUE(REPEAT_COUNT > 1);
        for (int i = 1; i < REPEAT_COUNT; ++i) {
            EXPECT_EQ(mfx_mem_ids[0], mfx_mem_ids[i]);
        }
    };

    MfxFrameConverterTest( { test_cache } );
}

typedef std::function<void (MfxFrameAllocator* allocator, MfxFramePoolAllocator* pool_allocator)> MfxFramePoolAllocatorTestStep;

static void MfxFramePoolAllocatorTest(const std::vector<MfxFramePoolAllocatorTestStep>& steps, int repeat_count = 1)
{
    std::shared_ptr<C2BlockPool> c2_allocator;
    c2_status_t res = C2_OK;
    std::shared_ptr<MfxC2ParamReflector> reflector = std::make_shared<MfxC2ParamReflector>();
    std::shared_ptr<const C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, {}, reflector, &res));
    EXPECT_EQ(res, C2_OK);

    res = GetCodec2BlockPool(C2BlockPool::BASIC_GRAPHIC,
        component, &c2_allocator);
    EXPECT_EQ(res, C2_OK);
    EXPECT_NE(c2_allocator, nullptr);

    std::unique_ptr<MfxDev> dev { new MfxDevVa(MfxDev::Usage::Decoder) };

    mfxStatus sts = dev->Init();
    EXPECT_EQ(MFX_ERR_NONE, sts);

    if (c2_allocator) {
        std::shared_ptr<MfxFrameAllocator> allocator = dev->GetFrameAllocator();
        EXPECT_NE(allocator, nullptr);
        std::shared_ptr<MfxFramePoolAllocator> pool_allocator = dev->GetFramePoolAllocator();
        EXPECT_NE(pool_allocator, nullptr);
        if (pool_allocator) {

            pool_allocator->SetC2Allocator(c2_allocator);

            for (int i = 0; i < repeat_count; ++i) {
                for (auto& step : steps) {
                    step(allocator.get(), pool_allocator.get());
                }
            }
        }
    }

    sts = dev->Close();
    EXPECT_EQ(MFX_ERR_NONE, sts);
}

// Tests a typical use sequence for MfxFramePoolAllocator.
// 1) Preallocate pool of frames through MfxFrameAllocator::AllocFrames.
// 2) Acquire C2 Graphic Blocks from the allocator, saves C2 handles and
// their wired MFX Mem IDs for future comparison.
// 3) Free C2 Graphic Blocks by releasing their shared_ptrs.
// 4) Acquire C2 Graphic Blocks again, check C2 handles and
// their wired MFX Mem IDs are the same as saved on step 2.
// 5) Reset allocator - release ownership of allocated C2 handles (no allocated any more).
// 6) Allocate again.
// 7) Check all handles are new.
TEST(MfxFramePoolAllocator, RetainHandles)
{
    const size_t FRAME_COUNT = 10;
    const int WIDTH = 1920;
    const int HEIGHT = 1080;
    const mfxU32 FOURCC = MFX_FOURCC_NV12;
    std::shared_ptr<C2GraphicBlock> c2_blocks[FRAME_COUNT];

    std::map<const C2Handle*, mfxHDL> handleC2ToMfx;

    mfxFrameAllocResponse response {};

    auto mfx_alloc = [&] (MfxFrameAllocator* allocator, MfxFramePoolAllocator*) {

        mfxFrameAllocRequest request {};
        request.Type = MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;
        request.NumFrameMin = FRAME_COUNT;
        request.NumFrameSuggested = FRAME_COUNT;
        InitFrameInfo(FOURCC, WIDTH, HEIGHT, &request.Info);

        mfxStatus sts = allocator->AllocFrames(&request, &response);
        EXPECT_EQ(sts, MFX_ERR_NONE);
        EXPECT_EQ(response.NumFrameActual, request.NumFrameMin);

        EXPECT_NE(response.mids, nullptr);
    };

    auto pool_alloc = [&] (MfxFrameAllocator* allocator, MfxFramePoolAllocator* pool_allocator) {
        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            c2_blocks[i] = pool_allocator->Alloc();
            EXPECT_NE(c2_blocks[i], nullptr);
            if (c2_blocks[i]) {
                const C2Handle* c2_handle = c2_blocks[i]->handle();
                mfxHDL mfx_handle {};
                mfxStatus sts = allocator->GetFrameHDL(response.mids[i], &mfx_handle);
                EXPECT_EQ(sts, MFX_ERR_NONE);
                handleC2ToMfx[c2_handle] = mfx_handle;
            }
        }
        EXPECT_EQ(handleC2ToMfx.size(), FRAME_COUNT);
    };

    auto pool_free = [&] (MfxFrameAllocator*, MfxFramePoolAllocator*) {
        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            c2_blocks[i].reset();
        }
    };

    auto pool_reset = [&] (MfxFrameAllocator*, MfxFramePoolAllocator* pool_allocator) {
        pool_allocator->Reset();
    };

    auto alloc_retains_handles = [&] (MfxFrameAllocator* allocator, MfxFramePoolAllocator* pool_allocator) {
        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            c2_blocks[i] = pool_allocator->Alloc();
            EXPECT_NE(c2_blocks[i], nullptr);
            if (c2_blocks[i]) {
                const C2Handle* c2_handle = c2_blocks[i]->handle();
                mfxHDL mfx_handle {};
                mfxStatus sts = allocator->GetFrameHDL(response.mids[i], &mfx_handle);
                EXPECT_EQ(sts, MFX_ERR_NONE);

                EXPECT_EQ(handleC2ToMfx[c2_handle], mfx_handle);
            }
        }
    };

    auto alloc_another_handles = [&] (MfxFrameAllocator*, MfxFramePoolAllocator* pool_allocator) {
        std::shared_ptr<C2GraphicBlock> c2_blocks_2[FRAME_COUNT];
        for (size_t i = 0; i < FRAME_COUNT; ++i) {
            c2_blocks_2[i] = pool_allocator->Alloc();
            EXPECT_NE(c2_blocks_2[i], nullptr);
            if(c2_blocks_2[i]) {
                const C2Handle* c2_handle = c2_blocks_2[i]->handle();
                EXPECT_EQ(handleC2ToMfx.find(c2_handle), handleC2ToMfx.end());
            }
        }
    };

    MfxFramePoolAllocatorTest( { mfx_alloc, pool_alloc, pool_free, alloc_retains_handles,
        pool_reset, mfx_alloc, alloc_another_handles } );
}

#endif

// Creates 2 graphic blocks, fill 1st with pattern,
// copy contents to 2nd and check pattern there.
TEST(C2Utils, CopyGraphicView)
{
    const int WIDTH = 1920;
    const int HEIGHT = 1080;

    do {
        c2_status_t res = C2_OK;

        std::shared_ptr<MfxC2ParamReflector> reflector = std::make_shared<MfxC2ParamReflector>();
        std::shared_ptr<const C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, {}, reflector, &res));
        std::shared_ptr<C2BlockPool> c2_allocator;
        GetCodec2BlockPool(C2BlockPool::BASIC_GRAPHIC, component, &c2_allocator);
        EXPECT_NE(c2_allocator, nullptr);
        if (!c2_allocator) break;

        std::shared_ptr<C2GraphicBlock> src_block, dst_block;

        for (auto block : {&src_block, &dst_block}) {
            c2_allocator->fetchGraphicBlock(WIDTH, HEIGHT, HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE }, block);
            EXPECT_NE(*block, nullptr);
        }
        if (src_block == nullptr || dst_block == nullptr) break;

        std::unique_ptr<C2GraphicView> src_view, dst_view;

        MapGraphicBlock(*src_block, MFX_SECOND_NS, &src_view);
        MapGraphicBlock(*dst_block, MFX_SECOND_NS, &dst_view);
        if (src_view == nullptr || dst_view == nullptr) break;

        FillFrameContents(WIDTH, HEIGHT, 0/*pattern seed*/, src_view->layout(), src_view->data());

        res = CopyGraphicView(src_view.get(), dst_view.get());
        EXPECT_EQ(res, C2_OK);

        CheckFrameContents(WIDTH, HEIGHT, 0/*pattern seed*/, dst_view->layout(), dst_view->data());

    } while(false);
}
