/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <android/hardware/graphics/bufferqueue/2.0/IGraphicBufferProducer.h>

#include <C2Buffer.h>
#include <C2PlatformSupport.h>

#include <functional>

class MfxC2BufferQueueBlockPool : public C2BlockPool {
public:
    MfxC2BufferQueueBlockPool(const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId);

    virtual ~MfxC2BufferQueueBlockPool() override;

    virtual C2Allocator::id_t getAllocatorId() const override {
        return android::C2PlatformAllocatorStore::BUFFERQUEUE;
    };

    virtual local_id_t getLocalId() const override {
       return local_id_;
    };

    virtual c2_status_t fetchGraphicBlock(
            uint32_t width,
            uint32_t height,
            uint32_t format,
            C2MemoryUsage usage,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) override;

    typedef std::function<void(uint64_t producer, int32_t slot, int64_t nsecs)> OnRenderCallback;

    /**
     * Sets render callback.
     *
     * \param renderCallbak callback to call for all dequeue buffer.
     */
    virtual void setRenderCallback(const OnRenderCallback &renderCallback = OnRenderCallback());

    typedef ::android::hardware::graphics::bufferqueue::V2_0::
            IGraphicBufferProducer HGraphicBufferProducer;
    /**
     * Configures an IGBP in order to create blocks. A newly created block is
     * dequeued from the configured IGBP. Unique Id of IGBP and the slot number of
     * blocks are passed via native_handle. Managing IGBP is responsibility of caller.
     * When IGBP is not configured, block will be created via allocator.
     * Since zero is not used for Unique Id of IGBP, if IGBP is not configured or producer
     * is configured as nullptr, unique id which is bundled in native_handle is zero.
     *
     * \param producer      the IGBP, which will be used to fetch blocks
     */
    virtual void configureProducer(const android::sp<HGraphicBufferProducer> &producer);

    virtual c2_status_t ImportHandle(
            const std::shared_ptr<C2GraphicBlock> block, buffer_handle_t *hndl);
    virtual c2_status_t requestNewBufferSet(int32_t bufferCount);

    virtual bool outputSurfaceSet(void);

private:
    const std::shared_ptr<C2Allocator> mAllocator;
    const local_id_t local_id_;

    class Impl;
    std::shared_ptr<Impl> impl_;

    friend struct MfxC2BufferQueueBlockPoolData;
};
