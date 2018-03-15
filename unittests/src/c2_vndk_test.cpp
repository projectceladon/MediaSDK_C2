/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

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

#include "gtest_emulation.h"
#include "mfx_defs.h"

#include <C2AllocatorIon.h>
#include <C2AllocatorGralloc.h>
#include <C2Buffer.h>
#include <C2BufferPriv.h>

#include <memory>

using namespace android;

class C2BufferTest
{
public:
    C2BufferTest()
        : mLinearAllocator(std::make_shared<C2AllocatorIon>()),
          mSize(0u),
          mAddr(nullptr),
          mGraphicAllocator(std::make_shared<C2AllocatorGralloc>()) {
    }

    ~C2BufferTest() = default;

    void allocateLinear(size_t capacity) {
        c2_status_t err = mLinearAllocator->newLinearAllocation(
                capacity,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
                &mLinearAllocation);
        if (err != C2_OK) {
            mLinearAllocation.reset();
            EXPECT_TRUE(false) << "C2Allocator::newLinearAllocation() failed: " << err;
        }
    }

    void mapLinear(size_t offset, size_t size, uint8_t **addr) {
        ASSERT_TRUE(mLinearAllocation);
        c2_status_t err = mLinearAllocation->map(
                offset,
                size,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
                // TODO: fence
                nullptr,
                &mAddr);
        if (err != C2_OK) {
            mAddr = nullptr;
            EXPECT_TRUE(false) << "C2LinearAllocation::map() failed: " << err;
        }
        ASSERT_NE(nullptr, mAddr);
        mSize = size;
        *addr = (uint8_t *)mAddr;
    }

    void unmapLinear() {
        ASSERT_TRUE(mLinearAllocation);
        ASSERT_NE(nullptr, mAddr);
        ASSERT_NE(0u, mSize);

        // TODO: fence
        ASSERT_EQ(C2_OK, mLinearAllocation->unmap(mAddr, mSize, nullptr));
        mSize = 0u;
        mAddr = nullptr;
    }

    std::shared_ptr<C2BlockPool> makeLinearBlockPool() {
        return std::make_shared<C2BasicLinearBlockPool>(mLinearAllocator);
    }

    void allocateGraphic(uint32_t width, uint32_t height) {
        c2_status_t err = mGraphicAllocator->newGraphicAllocation(
                width,
                height,
                HAL_PIXEL_FORMAT_YCBCR_420_888,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
                &mGraphicAllocation);
        if (err != C2_OK) {
            mGraphicAllocation.reset();
            EXPECT_TRUE(false) << "C2Allocator::newGraphicAllocation() failed: " << err;
        }
    }

    void mapGraphic(C2Rect rect, C2PlanarLayout *layout, uint8_t **addr) {
        ASSERT_TRUE(mGraphicAllocation);
        c2_status_t err = mGraphicAllocation->map(
                rect,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
                // TODO: fence
                nullptr,
                layout,
                addr);
        if (err != C2_OK) {
            addr[C2PlanarLayout::PLANE_Y] = nullptr;
            addr[C2PlanarLayout::PLANE_U] = nullptr;
            addr[C2PlanarLayout::PLANE_V] = nullptr;
            EXPECT_TRUE(false) << "C2GraphicAllocation::map() failed: " << err;
        }
    }

    void unmapGraphic() {
        ASSERT_TRUE(mGraphicAllocation);

        // TODO: fence
        ASSERT_EQ(C2_OK, mGraphicAllocation->unmap(nullptr));
    }

    std::shared_ptr<C2BlockPool> makeGraphicBlockPool() {
        return std::make_shared<C2BasicGraphicBlockPool>(mGraphicAllocator);
    }

private:
    std::shared_ptr<C2Allocator> mLinearAllocator;
    std::shared_ptr<C2LinearAllocation> mLinearAllocation;
    size_t mSize;
    void *mAddr;

    std::shared_ptr<C2Allocator> mGraphicAllocator;
    std::shared_ptr<C2GraphicAllocation> mGraphicAllocation;
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

    uint32_t pixel_formats[] { HAL_PIXEL_FORMAT_YCBCR_420_888, HAL_PIXEL_FORMAT_NV12_TILED_INTEL };

    for (uint32_t pixel_format : pixel_formats ) {

        std::shared_ptr<C2GraphicBlock> block;
        ASSERT_EQ(C2_OK, blockPool->fetchGraphicBlock(
                kWidth,
                kHeight,
                pixel_format,
                { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE },
                &block));
        ASSERT_TRUE(block);

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

        fillPlane({ 0, 0, kWidth, kHeight }, yInfo, y, 0);
        fillPlane({ 0, 0, kWidth, kHeight }, uInfo, u, 0);
        fillPlane({ 0, 0, kWidth, kHeight }, vInfo, v, 0);
        fillPlane({ kWidth / 4, kHeight / 4, kWidth / 2, kHeight / 2 }, yInfo, y, 0x12);
        fillPlane({ kWidth / 4, kHeight / 4, kWidth / 2, kHeight / 2 }, uInfo, u, 0x34);
        fillPlane({ kWidth / 4, kHeight / 4, kWidth / 2, kHeight / 2 }, vInfo, v, 0x56);

        C2Fence fence;
        C2ConstGraphicBlock constBlock = block->share(
                { 0, 0, kWidth, kHeight }, fence);
        block.reset();

        C2Acquirable<const C2GraphicView> constViewHolder = constBlock.map();
        const C2GraphicView constGraphicView = constViewHolder.get();
        ASSERT_EQ(C2_OK, constGraphicView.error());
        ASSERT_EQ(kWidth, constGraphicView.width());
        ASSERT_EQ(kHeight, constGraphicView.height());

        const uint8_t *const *constData = constGraphicView.data();
        layout = graphicView.layout();
        ASSERT_NE(nullptr, constData);

        const uint8_t *cy = constData[C2PlanarLayout::PLANE_Y];
        yInfo = layout.planes[C2PlanarLayout::PLANE_Y];
        const uint8_t *cu = constData[C2PlanarLayout::PLANE_U];
        uInfo = layout.planes[C2PlanarLayout::PLANE_U];
        const uint8_t *cv = constData[C2PlanarLayout::PLANE_V];
        vInfo = layout.planes[C2PlanarLayout::PLANE_V];

        ASSERT_TRUE(verifyPlane({ kWidth / 4, kHeight / 4, kWidth / 2, kHeight / 2 }, yInfo, cy, 0x12));
        ASSERT_TRUE(verifyPlane({ kWidth / 4, kHeight / 4, kWidth / 2, kHeight / 2 }, uInfo, cu, 0x34));
        ASSERT_TRUE(verifyPlane({ kWidth / 4, kHeight / 4, kWidth / 2, kHeight / 2 }, vInfo, cv, 0x56));
        ASSERT_TRUE(verifyPlane({ 0, 0, kWidth, kHeight / 4 }, yInfo, cy, 0));
        ASSERT_TRUE(verifyPlane({ 0, 0, kWidth, kHeight / 4 }, uInfo, cu, 0));
        ASSERT_TRUE(verifyPlane({ 0, 0, kWidth, kHeight / 4 }, vInfo, cv, 0));
        ASSERT_TRUE(verifyPlane({ 0, 0, kWidth / 4, kHeight }, yInfo, cy, 0));
        ASSERT_TRUE(verifyPlane({ 0, 0, kWidth / 4, kHeight }, uInfo, cu, 0));
        ASSERT_TRUE(verifyPlane({ 0, 0, kWidth / 4, kHeight }, vInfo, cv, 0));
    }
}
