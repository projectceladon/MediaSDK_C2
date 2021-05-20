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

/*
 * Copyright 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// The contents of this file was copied
// from AOSP frameworks/av/media/libstagefright/codec2/tests/vndk/C2BufferTest.cpp
// and modified then.

#include <gtest/gtest.h>
#include "mfx_defs.h"

#include <C2Buffer.h>
#include <C2PlatformSupport.h>
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_gralloc_allocator.h"

#include <memory>

using namespace android;

#define MOCK_COMPONENT_ENC "c2.intel.mock.encoder"
#define MOCK_COMPONENT MOCK_COMPONENT_ENC // use encoder for common tests

class C2BufferTest
{
public:
    C2BufferTest() = default;
    ~C2BufferTest() = default;

    std::shared_ptr<C2BlockPool> makeLinearBlockPool() {

        std::shared_ptr<MfxC2ParamReflector> reflector = std::make_shared<MfxC2ParamReflector>();
        std::shared_ptr<const C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, {}, reflector, nullptr));

        std::shared_ptr<C2BlockPool> block_pool;
        GetCodec2BlockPool(C2BlockPool::BASIC_LINEAR, component, &block_pool);
        return block_pool;
    }

    std::shared_ptr<C2BlockPool> makeGraphicBlockPool() {

        std::shared_ptr<MfxC2ParamReflector> reflector = std::make_shared<MfxC2ParamReflector>();
        std::shared_ptr<const C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, {}, reflector, nullptr));

        std::shared_ptr<C2BlockPool> block_pool;
        GetCodec2BlockPool(C2BlockPool::BASIC_GRAPHIC, component, &block_pool);
        return block_pool;
    }
};

TEST(C2BufferTest, BlockPoolTest) {
    constexpr size_t kCapacity = 1024u * 1024u;

    C2BufferTest test;
    std::shared_ptr<C2BlockPool> blockPool(test.makeLinearBlockPool());

    std::shared_ptr<C2LinearBlock> block;
    ASSERT_EQ(C2_OK, blockPool->fetchLinearBlock(
            kCapacity,
            { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
            &block));
    ASSERT_TRUE(block);

    C2Acquirable<C2WriteView> writeViewHolder = block->map();
    C2WriteView writeView = writeViewHolder.get();
    ASSERT_EQ(C2_OK, writeView.error());
    ASSERT_EQ(kCapacity, writeView.capacity());
    ASSERT_EQ(0u, writeView.offset());
    ASSERT_EQ(kCapacity, writeView.size());

    uint8_t *data = writeView.data();
    ASSERT_NE(nullptr, data);
    for (size_t i = 0; i < writeView.size(); ++i) {
        data[i] = i % 100u;
    }

    C2Fence fence;
    C2ConstLinearBlock constBlock = block->share(
            kCapacity / 3, kCapacity / 3, fence);

    C2Acquirable<C2ReadView> readViewHolder = constBlock.map();
    C2ReadView readView = readViewHolder.get();
    ASSERT_EQ(C2_OK, readView.error());
    ASSERT_EQ(kCapacity / 3, readView.capacity());

    // TODO: fence
    const uint8_t *constData = readView.data();
    ASSERT_NE(nullptr, constData);
    for (size_t i = 0; i < readView.capacity(); ++i) {
        ASSERT_EQ((i + kCapacity / 3) % 100u, constData[i]) << " at i = " << i
                << "; data = " << static_cast<void *>(data)
                << "; constData = " << static_cast<const void *>(constData);
    }

    readView = readView.subView(333u, 100u);
    ASSERT_EQ(C2_OK, readView.error());
    ASSERT_EQ(100u, readView.capacity());

    constData = readView.data();
    ASSERT_NE(nullptr, constData);
    for (size_t i = 0; i < readView.capacity(); ++i) {
        ASSERT_EQ((i + 333u + kCapacity / 3) % 100u, constData[i]) << " at i = " << i;
    }
}

void fillPlane(const C2Rect rect, const C2PlaneInfo info, uint8_t *addr, uint8_t value) {
    for (uint32_t row = 0; row < rect.height / info.rowSampling; ++row) {
        int32_t rowOffset = (row + rect.top / info.rowSampling) * info.rowInc;
        for (uint32_t col = 0; col < rect.width / info.colSampling; ++col) {
            int32_t colOffset = (col + rect.left / info.colSampling) * info.colInc;
            addr[rowOffset + colOffset] = value;
        }
    }
}

bool verifyPlane(const C2Rect rect, const C2PlaneInfo info, const uint8_t *addr, uint8_t value) {
    for (uint32_t row = 0; row < rect.height / info.rowSampling; ++row) {
        int32_t rowOffset = (row + rect.top / info.rowSampling) * info.rowInc;
        for (uint32_t col = 0; col < rect.width / info.colSampling; ++col) {
            int32_t colOffset = (col + rect.left / info.colSampling) * info.colInc;
            if (addr[rowOffset + colOffset] != value) {
                return false;
            }
        }
    }
    return true;
}

TEST(C2BufferTest, GraphicBlockPoolTest) {
    constexpr uint32_t kWidth = 320;
    constexpr uint32_t kHeight = 240;

    C2BufferTest test;

    std::shared_ptr<C2BlockPool> blockPool(test.makeGraphicBlockPool());

    auto fill_planes = [&] (std::shared_ptr<C2GraphicBlock> block) {

        C2Acquirable<C2GraphicView> graphicViewHolder = block->map();
        C2GraphicView graphicView = graphicViewHolder.get();
        ASSERT_EQ(C2_OK, graphicView.error());
        ASSERT_EQ(kWidth, graphicView.width());
        ASSERT_EQ(kHeight, graphicView.height());

        uint8_t *const *data = graphicView.data();
        C2PlanarLayout layout = graphicView.layout();
        ASSERT_NE(nullptr, data);

        uint8_t *y = data[C2PlanarLayout::PLANE_Y];
        C2PlaneInfo yInfo = layout.planes[C2PlanarLayout::PLANE_Y];
        uint8_t *u = data[C2PlanarLayout::PLANE_U];
        C2PlaneInfo uInfo = layout.planes[C2PlanarLayout::PLANE_U];
        uint8_t *v = data[C2PlanarLayout::PLANE_V];
        C2PlaneInfo vInfo = layout.planes[C2PlanarLayout::PLANE_V];

        fillPlane({ kWidth, kHeight }, yInfo, y, 0);
        fillPlane({ kWidth, kHeight }, uInfo, u, 0);
        fillPlane({ kWidth, kHeight }, vInfo, v, 0);
        fillPlane(C2Rect(kWidth / 2, kHeight / 2).at(kWidth / 4, kHeight / 4), yInfo, y, 0x12);
        fillPlane(C2Rect(kWidth / 2, kHeight / 2).at(kWidth / 4, kHeight / 4), uInfo, u, 0x34);
        fillPlane(C2Rect(kWidth / 2, kHeight / 2).at(kWidth / 4, kHeight / 4), vInfo, v, 0x56);
    };

    auto verify_planes = [&] (std::shared_ptr<C2GraphicBlock> block) {

        C2Fence fence;
        C2ConstGraphicBlock constBlock = block->share(
                { kWidth, kHeight }, fence);
        block.reset();

        C2Acquirable<const C2GraphicView> constViewHolder = constBlock.map();
        const C2GraphicView constGraphicView = constViewHolder.get();
        ASSERT_EQ(C2_OK, constGraphicView.error());
        ASSERT_EQ(kWidth, constGraphicView.width());
        ASSERT_EQ(kHeight, constGraphicView.height());

        const uint8_t *const *constData = constGraphicView.data();
        C2PlanarLayout layout = constGraphicView.layout();
        ASSERT_NE(nullptr, constData);

        const uint8_t *cy = constData[C2PlanarLayout::PLANE_Y];
        C2PlaneInfo yInfo = layout.planes[C2PlanarLayout::PLANE_Y];
        const uint8_t *cu = constData[C2PlanarLayout::PLANE_U];
        C2PlaneInfo uInfo = layout.planes[C2PlanarLayout::PLANE_U];
        const uint8_t *cv = constData[C2PlanarLayout::PLANE_V];
        C2PlaneInfo vInfo = layout.planes[C2PlanarLayout::PLANE_V];

        ASSERT_TRUE(verifyPlane(C2Rect(kWidth / 2, kHeight / 2).at(kWidth / 4, kHeight / 4), yInfo, cy, 0x12));
        ASSERT_TRUE(verifyPlane(C2Rect(kWidth / 2, kHeight / 2).at(kWidth / 4, kHeight / 4), uInfo, cu, 0x34));
        ASSERT_TRUE(verifyPlane(C2Rect(kWidth / 2, kHeight / 2).at(kWidth / 4, kHeight / 4), vInfo, cv, 0x56));
        ASSERT_TRUE(verifyPlane({ kWidth, kHeight / 4 }, yInfo, cy, 0));
        ASSERT_TRUE(verifyPlane({ kWidth, kHeight / 4 }, uInfo, cu, 0));
        ASSERT_TRUE(verifyPlane({ kWidth, kHeight / 4 }, vInfo, cv, 0));
        ASSERT_TRUE(verifyPlane({ kWidth / 4, kHeight }, yInfo, cy, 0));
        ASSERT_TRUE(verifyPlane({ kWidth / 4, kHeight }, uInfo, cu, 0));
        ASSERT_TRUE(verifyPlane({ kWidth / 4, kHeight }, vInfo, cv, 0));
    };

    uint32_t pixel_formats[] {
        HAL_PIXEL_FORMAT_YCBCR_420_888,
        HAL_PIXEL_FORMAT_NV12_TILED_INTEL
    };

    for (uint32_t pixel_format : pixel_formats ) {

        std::shared_ptr<C2GraphicBlock> block;
        ASSERT_EQ(C2_OK, blockPool->fetchGraphicBlock(
                kWidth,
                kHeight,
                pixel_format,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
                &block));
        ASSERT_TRUE(block);

        fill_planes(block);
        verify_planes(std::move(block)); // move to allow verify_planes free block
    }
}

namespace map_test
{
const uint32_t WIDTH = 176;
const uint32_t HEIGHT = 144;
const uint32_t PIXEL_FORMAT = HAL_PIXEL_FORMAT_NV12_TILED_INTEL;

// Alloc/Map wrapper for C2
class C2
{
public:
    using Block = std::shared_ptr<C2GraphicBlock>;
    using View = C2GraphicView;

    C2()
    {
        C2BufferTest test;
        block_pool_ = test.makeGraphicBlockPool();
    }

    void Alloc(std::list<Block>* dst)
    {
        std::shared_ptr<C2GraphicBlock> block;
        EXPECT_EQ(C2_OK, block_pool_->fetchGraphicBlock(WIDTH, HEIGHT, PIXEL_FORMAT,
            { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
            &block));
        EXPECT_TRUE(block);
        if (block) dst->push_back(std::move(block));
    }

    void Map(Block& block, std::list<View>* dst)
    {
        C2Acquirable<C2GraphicView> acq_graph_view = block->map();
        C2GraphicView view{acq_graph_view.get()};
        EXPECT_EQ(view.error(), C2_OK);
        c2_status_t res = C2_OK; //acq_graph_view.wait(MFX_SECOND_NS); wait not supported yet
        //EXPECT_EQ(res, C2_OK);

        if (view.error() == C2_OK && res == C2_OK)
            dst->emplace_back(view);
    }

private:
    std::shared_ptr<C2BlockPool> block_pool_;
};

// Alloc/Map wrapper for gralloc
class Gralloc
{
public:
    class Block
    {
    public:
        Block() = default;
        MFX_CLASS_NO_COPY(Block);
        ~Block() { if (deleter_) deleter_(handle_); }

    private:
        buffer_handle_t handle_{};
        std::function<void (buffer_handle_t)> deleter_;
        friend class Gralloc;
    };

    class View
    {
    public:
        View() = default;
        MFX_CLASS_NO_COPY(View);
        ~View() { if (deleter_) deleter_(handle_); }

        C2PlanarLayout layout() const { return layout_; }
        const uint8_t *const *data() const { return data_; }

    private:
        uint8_t* data_[C2PlanarLayout::MAX_NUM_PLANES]{};
        C2PlanarLayout layout_;
        buffer_handle_t handle_{};
        std::function<void (buffer_handle_t)> deleter_;
        friend class Gralloc;
    };

    Gralloc()
    {
        c2_status_t res = MfxGrallocAllocator::Create(&allocator);
        EXPECT_EQ(res, C2_OK);
        EXPECT_NE(allocator, nullptr);
    }

    void Alloc(std::list<Block>* dst)
    {
        buffer_handle_t handle{};
        EXPECT_EQ(C2_OK, allocator->Alloc(WIDTH, HEIGHT, &handle));
        EXPECT_TRUE(handle);
        if (handle) {
            dst->emplace_back();
            dst->back().handle_ = handle;
            dst->back().deleter_ = [this](buffer_handle_t h) {
                c2_status_t res = allocator->Free(h);
                EXPECT_EQ(res, C2_OK);
            };
        }
    }

    void Map(Block& block, std::list<View>* dst)
    {
        dst->emplace_back(); // construct view in place

        c2_status_t res = allocator->LockFrame(block.handle_, dst->back().data_, &(dst->back().layout_));
        EXPECT_EQ(res, C2_OK);

        if (res == C2_OK) {
            dst->back().handle_ = block.handle_;
            dst->back().deleter_ = [this](buffer_handle_t h) {
                c2_status_t res = allocator->UnlockFrame(h);
                EXPECT_EQ(res, C2_OK);
            };
        } else {
            dst->pop_back(); // remove in case of failure
        }
    }

private:
    std::unique_ptr<MfxGrallocAllocator> allocator;
};

} // namespace map_test

// Test runs multiple threads, allocates graphic blocks there and maps them simultanously.
// Memory regions where blocks are mapped to are checked for mutual intersections.
// The tests came as a result of investigation random DecodeBitExact crashes/output corruptions
// caused by IMapper mapping different blocks into the same memory if called from
// different threads simultanously.
template<typename Allocator>
void NotOverlappedTest()
{
    const int BLOCK_COUNT = 16;
    const int PASS_COUNT = 1000;
    const int THREAD_COUNT = 10;
    static_assert(THREAD_COUNT >= 2, "Test requires at least 2 threads");

    std::thread threads[THREAD_COUNT];

    struct MemoryRegion
    {
        const uint8_t* start{};
        size_t length{};
    };

    std::list<MemoryRegion> regions[THREAD_COUNT];
    std::atomic<int> regions_ready_count{};
    std::atomic<int> main_pass_index{};
    std::atomic<int> compare_count{};

    Allocator allocator;

    auto allocate_blocks = [&]()->std::list<typename Allocator::Block> {
        std::list<typename Allocator::Block> blocks;
        for (int i = 0; i < BLOCK_COUNT; ++i) {
            allocator.Alloc(&blocks);
        }
        return blocks;
    };

    auto map_blocks = [&](
        std::list<typename Allocator::Block>& blocks)->std::list<typename Allocator::View> {

        std::list<typename Allocator::View> views;

        for (auto& block : blocks) {
            allocator.Map(block, &views);
        }
        return views;
    };

    auto get_memory_regions = [&](const C2PlanarLayout& layout, const uint8_t *const *data) {
        std::list<MemoryRegion> regions;

        struct Offsets
        {
            ssize_t min{};
            ssize_t max{};
        };

        std::map<uint32_t, Offsets> root_plane_offsets;
        for (uint32_t plane_index = 0; plane_index < layout.numPlanes; ++plane_index) {
            const C2PlaneInfo& plane{layout.planes[plane_index]};
            ssize_t min_offset = plane.offset + plane.minOffset(0, 0);
            ssize_t max_offset = plane.offset +
                plane.maxOffset(map_test::WIDTH / plane.colSampling, map_test::HEIGHT / plane.rowSampling);
            auto it = root_plane_offsets.find(plane.rootIx);
            if (it != root_plane_offsets.end()) {
                Offsets& offsets = it->second;
                if (min_offset < offsets.min) offsets.min = min_offset;
                if (max_offset > offsets.max) offsets.max = max_offset;
            } else {
                root_plane_offsets.emplace(plane.rootIx, Offsets({min_offset, max_offset}));
            }
        }

        for (const auto& item : root_plane_offsets) {
            uint32_t root_index = item.first;
            const Offsets& offsets = item.second;

            regions.emplace_back(
                MemoryRegion{data[root_index] + offsets.min, (size_t)(offsets.max - offsets.min)});
        }

        return regions;
    };

    auto thread_func = [&] (size_t thread_index) {
        // allocate BLOCK_COUNT in local storage
        std::list<typename Allocator::Block> blocks = allocate_blocks();

        for (int pass_index = 1; pass_index <= PASS_COUNT; ++pass_index) {

            while (main_pass_index.load() != pass_index); // wait for iteration start

            // list is outside mapping loop to keep blocks mapped
            std::list<typename Allocator::View> views = map_blocks(blocks); // map all local blocks

            for (const auto& view : views) {
                regions[thread_index].splice(regions[thread_index].end(),
                    get_memory_regions(view.layout(), view.data()));
            }

            // signal memory list is ready
            ++regions_ready_count;

            // wait for comparison
            while (compare_count.load() != pass_index);
            // unmap is done automatically by views destruction
        }
    };

    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads[i] = std::thread(thread_func, i);
    }

    for (int i = 0; i < PASS_COUNT; ++i) {

        ++main_pass_index; // signal of iteration start

        // wait for lists are ready
        while (regions_ready_count.load() != THREAD_COUNT);

        // check memory region lists for intersections
        std::list<MemoryRegion> all_regions; // check every region against this list and insert in there
        for (int j = 0; j < THREAD_COUNT; ++j) {
            for (const MemoryRegion& region : regions[j]) {
                auto intersects = [&region] (const MemoryRegion& other)->bool {
                    return (region.start < other.start + other.length) &&
                        (other.start < region.start + region.length);
                };
                EXPECT_EQ(std::find_if(all_regions.begin(), all_regions.end(), intersects), all_regions.end());
                all_regions.push_back(region);
            }
            regions[j].clear();
        }

        regions_ready_count.store(0);

        ++compare_count; // signal comparison is done
    }

    // join threads
    for (int i = 0; i < THREAD_COUNT; ++i) {
        threads[i].join();
    }
}

// Tests overlapping for C2GraphicBlock.
TEST(C2BufferTest, C2GraphicMappingNotOverlappedTest)
{
    NotOverlappedTest<map_test::C2>();
}

// Tests overlapping for gralloc handles.
TEST(C2BufferTest, GrallocMappingNotOverlappedTest)
{
    NotOverlappedTest<map_test::Gralloc>();
}
