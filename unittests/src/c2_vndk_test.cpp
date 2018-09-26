/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

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

#include <memory>

using namespace android;

#define MOCK_COMPONENT_ENC "C2.MockComponent.Enc"
#define MOCK_COMPONENT MOCK_COMPONENT_ENC // use encoder for common tests

class C2BufferTest
{
public:
    C2BufferTest() = default;
    ~C2BufferTest() = default;

    std::shared_ptr<C2BlockPool> makeLinearBlockPool() {

        std::shared_ptr<const C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, {}, nullptr));

        std::shared_ptr<C2BlockPool> block_pool;
        GetCodec2BlockPool(C2BlockPool::BASIC_LINEAR, component, &block_pool);
        return block_pool;
    }

    std::shared_ptr<C2BlockPool> makeGraphicBlockPool() {

        std::shared_ptr<const C2Component> component(MfxCreateC2Component(MOCK_COMPONENT, {}, nullptr));

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

// Test runs multiple threads, allocates graphic blocks there and maps them simultanously.
// Memory regions where blocks are mapped to are checked for mutual intersections.
// The tests came as a result of investigation random DecodeBitExact crashes/output corruptions
// caused by IMapper mapping different blocks into the same memory if called from
// different threads simultanously.
TEST(C2BufferTest, GraphicMappingNotOverlappedTest)
{
    const uint32_t WIDTH = 176;
    const uint32_t HEIGHT = 144;
    const uint32_t PIXEL_FORMAT = HAL_PIXEL_FORMAT_NV12_TILED_INTEL;
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

    C2BufferTest test;
    std::shared_ptr<C2BlockPool> blockPool(test.makeGraphicBlockPool());

    auto allocate_blocks = [&]()->std::vector<std::shared_ptr<C2GraphicBlock>> {
        std::vector<std::shared_ptr<C2GraphicBlock>> blocks(BLOCK_COUNT);
        for (int i = 0; i < BLOCK_COUNT; ++i) {
            EXPECT_EQ(C2_OK, blockPool->fetchGraphicBlock(WIDTH, HEIGHT, PIXEL_FORMAT,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
                &blocks[i]));
            EXPECT_TRUE(blocks[i]);
        }
        return blocks;
    };

    auto map_blocks = [&](
        const std::vector<std::shared_ptr<C2GraphicBlock>>& blocks)->std::list<C2GraphicView> {

        std::list<C2GraphicView> views;

        for (const auto& block : blocks) {
            C2Acquirable<C2GraphicView> acq_graph_view = block->map();
            C2GraphicView view{acq_graph_view.get()};
            EXPECT_EQ(view.error(), C2_OK);
            EXPECT_EQ(acq_graph_view.wait(MFX_SECOND_NS), C2_OK);
            views.emplace_back(view); // keep in list to be mapped until end of pass
        }
        return views;
    };

    auto get_memory_regions = [&](const std::list<C2GraphicView>& views) {
        std::list<MemoryRegion> regions;

        for (auto& view : views) {
            struct Offsets
            {
                ssize_t min{};
                ssize_t max{};
            };

            C2PlanarLayout layout = view.layout();
            std::map<uint32_t, Offsets> root_plane_offsets;
            for (uint32_t plane_index = 0; plane_index < layout.numPlanes; ++plane_index) {
                const C2PlaneInfo& plane{layout.planes[plane_index]};
                ssize_t min_offset = plane.offset + plane.minOffset(0, 0);
                ssize_t max_offset = plane.offset +
                    plane.maxOffset(WIDTH / plane.colSampling, HEIGHT / plane.rowSampling);
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
                    MemoryRegion{view.data()[root_index] + offsets.min, (size_t)(offsets.max - offsets.min)});
            }
        }
        return regions;
    };

    auto thread_func = [&] (size_t thread_index) {
        // allocate BLOCK_COUNT in local storage
        std::vector<std::shared_ptr<C2GraphicBlock>> blocks = allocate_blocks();

        for (int pass_index = 1; pass_index <= PASS_COUNT; ++pass_index) {

            while (main_pass_index.load() != pass_index); // wait for iteration start

            // list is outside mapping loop to keep blocks mapped
            std::list<C2GraphicView> views = map_blocks(blocks); // map all local blocks

            regions[thread_index] = get_memory_regions(views);

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
