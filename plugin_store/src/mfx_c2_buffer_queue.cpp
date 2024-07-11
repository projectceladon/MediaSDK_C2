// Copyright (c) 2021 Intel Corporation
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


#include <utils/Log.h>

#include <ui/BufferQueueDefs.h>
#include <ui/GraphicBuffer.h>
#include <ui/Fence.h>

#include <types.h>
#include <hidl/HidlSupport.h>

#include <C2BlockInternal.h>
#include <C2AllocatorGralloc.h>

#include <list>
#include <map>
#include <mutex>

#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_buffer_queue.h"

using ::android::BufferQueueDefs::NUM_BUFFER_SLOTS;
using ::android::C2AndroidMemoryUsage;
using ::android::Fence;
using ::android::GraphicBuffer;
using ::android::sp;
using ::android::status_t;
using ::android::wp;
using ::android::hardware::hidl_handle;
using ::android::hardware::Return;

using HBuffer = ::android::hardware::graphics::common::V1_2::HardwareBuffer;
using HStatus = ::android::hardware::graphics::bufferqueue::V2_0::Status;
using ::android::hardware::graphics::bufferqueue::V2_0::utils::b2h;
using ::android::hardware::graphics::bufferqueue::V2_0::utils::h2b;
using ::android::hardware::graphics::bufferqueue::V2_0::utils::HFenceWrapper;

using HGraphicBufferProducer = ::android::hardware::graphics::bufferqueue::V2_0
        ::IGraphicBufferProducer;
using namespace android;

struct MfxC2BufferQueueBlockPoolData : public _C2BlockPoolData {

    bool held;
    bool local;
    uint32_t generation;
    uint64_t bqId;
    int32_t bqSlot;
    bool transfer; // local transfer to remote
    bool attach; // attach on remote
    bool display; // display on remote;
    std::weak_ptr<int> owner;
    sp<HGraphicBufferProducer> igbp;
    std::shared_ptr<MfxC2BufferQueueBlockPool::Impl> localPool;
    mutable std::mutex lock;

    virtual type_t getType() const override {
        return TYPE_BUFFERQUEUE;
    }

    // Create a remote BlockPoolData.
    MfxC2BufferQueueBlockPoolData(
            uint32_t generation, uint64_t bqId, int32_t bqSlot,
            const std::shared_ptr<int> &owner,
            const sp<HGraphicBufferProducer>& producer);

    // Create a local BlockPoolData.
    MfxC2BufferQueueBlockPoolData(
            uint32_t generation, uint64_t bqId, int32_t bqSlot,
            const std::shared_ptr<MfxC2BufferQueueBlockPool::Impl>& pool);

    virtual ~MfxC2BufferQueueBlockPoolData() override;

    int migrate(const sp<HGraphicBufferProducer>& producer,
                uint32_t toGeneration, uint64_t toBqId,
                sp<GraphicBuffer> *buffers, uint32_t oldGeneration);
};

namespace {

int64_t getTimestampNow() {
    int64_t stamp;
    struct timespec ts;
    // TODO: CLOCK_MONOTONIC_COARSE?
    clock_gettime(CLOCK_MONOTONIC, &ts);
    stamp = ts.tv_nsec / 1000;
    stamp += (ts.tv_sec * 1000000LL);
    return stamp;
}

bool getGenerationNumber(const sp<HGraphicBufferProducer> &producer,
                         uint32_t *generation) {
    MFX_DEBUG_TRACE_FUNC;
    status_t status{};
    int slot{};
    bool bufferNeedsReallocation{};
    sp<Fence> fence = new Fence();

    using Input = HGraphicBufferProducer::DequeueBufferInput;
    using Output = HGraphicBufferProducer::DequeueBufferOutput;
    Return<void> transResult = producer->dequeueBuffer(
            Input{640, 480, HAL_PIXEL_FORMAT_YCBCR_420_888, 0},
            [&status, &slot, &bufferNeedsReallocation, &fence]
            (HStatus hStatus, int32_t hSlot, Output const& hOutput) {
                slot = static_cast<int>(hSlot);
                if (!h2b(hStatus, &status) || !h2b(hOutput.fence, &fence)) {
                    status = ::android::BAD_VALUE;
                } else {
                    bufferNeedsReallocation =
                            hOutput.bufferNeedsReallocation;
                }
            });
    if (!transResult.isOk() || status != android::OK) {
        return false;
    }
    HFenceWrapper hFenceWrapper{};
    if (!b2h(fence, &hFenceWrapper)) {
        (void)producer->detachBuffer(static_cast<int32_t>(slot)).isOk();
        MFX_DEBUG_TRACE_PRINTF("Invalid fence received from dequeueBuffer.");
        return false;
    }
    sp<GraphicBuffer> slotBuffer = new GraphicBuffer();
    // This assumes requestBuffer# returns an existing allocation
    // instead of a new allocation.
    transResult = producer->requestBuffer(
            slot,
            [&status, &slotBuffer, &generation](
                    HStatus hStatus,
                    HBuffer const& hBuffer,
                    uint32_t generationNumber){
                if (h2b(hStatus, &status) &&
                        h2b(hBuffer, &slotBuffer) &&
                        slotBuffer) {
                    *generation = generationNumber;
                    slotBuffer->setGenerationNumber(generationNumber);
                } else {
                    status = android::BAD_VALUE;
                }
            });
    if (!transResult.isOk()) {
        return false;
    } else if (status != android::NO_ERROR) {
        (void)producer->detachBuffer(static_cast<int32_t>(slot)).isOk();
        return false;
    }
    (void)producer->detachBuffer(static_cast<int32_t>(slot)).isOk();
    return true;
}

};

class MfxC2BufferQueueBlockPool::Impl
        : public std::enable_shared_from_this<MfxC2BufferQueueBlockPool::Impl> {
private:
    c2_status_t fetchFromIgbp_l(
            uint32_t width,
            uint32_t height,
            uint32_t format,
            C2MemoryUsage usage,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {
        MFX_DEBUG_TRACE_FUNC;
        // We have an IGBP now.
        C2AndroidMemoryUsage androidUsage = usage;
        status_t status{};
        int slot{};
        bool bufferNeedsReallocation{};
        sp<Fence> fence = new Fence();

        { // Call dequeueBuffer().
            using Input = HGraphicBufferProducer::DequeueBufferInput;
            using Output = HGraphicBufferProducer::DequeueBufferOutput;
            Return<void> transResult = mProducer->dequeueBuffer(
                    Input{
                        width,
                        height,
                        format,
                        androidUsage.asGrallocUsage()},
                    [&status, &slot, &bufferNeedsReallocation,
                     &fence](HStatus hStatus,
                             int32_t hSlot,
                             Output const& hOutput) {
                        slot = static_cast<int>(hSlot);
                        if (!h2b(hStatus, &status) ||
                                !h2b(hOutput.fence, &fence)) {
                            status = ::android::BAD_VALUE;
                        } else {
                            bufferNeedsReallocation =
                                    hOutput.bufferNeedsReallocation;
                        }
                    });
            if (!transResult.isOk() || status != android::OK) {
                if (transResult.isOk()) {
                    ++mDqFailure;
                    if (status == android::INVALID_OPERATION ||
                        status == android::TIMED_OUT ||
                        status == android::WOULD_BLOCK) {
                        // Dequeue buffer is blocked temporarily. Retrying is
                        // required.
                        return C2_BLOCKING;
                    }
                }
                MFX_DEBUG_TRACE_PRINTF("cannot dequeue buffer %d", status);
                return C2_BAD_VALUE;
            }
            mDqFailure = 0;
            mLastDqTs = getTimestampNow();
        }
        HFenceWrapper hFenceWrapper{};
        if (!b2h(fence, &hFenceWrapper)) {
            MFX_DEBUG_TRACE_PRINTF("Invalid fence received from dequeueBuffer.");
            return C2_BAD_VALUE;
        }
        MFX_DEBUG_TRACE_PRINTF("dequeued a buffer successfully");
        if (fence) {
            static constexpr int kFenceWaitTimeMs = 10;

            status_t status = fence->wait(kFenceWaitTimeMs);
            if (status == -ETIME) {
                // fence is not signalled yet.
                (void)mProducer->cancelBuffer(slot, hFenceWrapper.getHandle()).isOk();
                return C2_BLOCKING;
            }
            if (status != android::NO_ERROR) {
                MFX_DEBUG_TRACE_PRINTF("buffer fence wait error %d", status);
                (void)mProducer->cancelBuffer(slot, hFenceWrapper.getHandle()).isOk();
                return C2_BAD_VALUE;
            } else if (mRenderCallback) {
                nsecs_t signalTime = fence->getSignalTime();
                if (signalTime >= 0 && signalTime < INT64_MAX) {
                    mRenderCallback(mProducerId, slot, signalTime);
                } else {
                    MFX_DEBUG_TRACE_PRINTF("got fence signal time of %lld", (long long)signalTime);
                }
            }
        }

        sp<GraphicBuffer> &slotBuffer = mBuffers[slot];
        uint32_t outGeneration;
        if (bufferNeedsReallocation || !slotBuffer) {
            if (!slotBuffer) {
                slotBuffer = new GraphicBuffer();
            }
            // N.B. This assumes requestBuffer# returns an existing allocation
            // instead of a new allocation.
            Return<void> transResult = mProducer->requestBuffer(
                    slot,
                    [&status, &slotBuffer, &outGeneration](
                            HStatus hStatus,
                            HBuffer const& hBuffer,
                            uint32_t generationNumber){
                        if (h2b(hStatus, &status) &&
                                h2b(hBuffer, &slotBuffer) &&
                                slotBuffer) {
                            slotBuffer->setGenerationNumber(generationNumber);
                            outGeneration = generationNumber;
                        } else {
                            status = android::BAD_VALUE;
                        }
                    });
            if (!transResult.isOk()) {
                slotBuffer.clear();
                return C2_BAD_VALUE;
            } else if (status != android::NO_ERROR) {
                slotBuffer.clear();
                (void)mProducer->cancelBuffer(slot, hFenceWrapper.getHandle()).isOk();
                return C2_BAD_VALUE;
            }
            if (mGeneration == 0) {
                // getting generation # lazily due to dequeue failure.
                mGeneration = outGeneration;
            }
        }
        if (slotBuffer) {
            MFX_DEBUG_TRACE_PRINTF("buffer wraps %llu %d, handle:%p", (unsigned long long)mProducerId, slot,slotBuffer->handle);
            C2Handle *c2Handle = WrapNativeCodec2GrallocHandle(
                    slotBuffer->handle,
                    slotBuffer->width,
                    slotBuffer->height,
                    slotBuffer->format,
                    slotBuffer->usage,
                    slotBuffer->stride,
                    slotBuffer->getGenerationNumber(),
                    mProducerId, slot);
            if (c2Handle) {
                std::shared_ptr<C2GraphicAllocation> alloc;
                c2_status_t err = mAllocator->priorGraphicAllocation(c2Handle, &alloc);
                if (err != C2_OK) {
                    native_handle_close(c2Handle);
                    native_handle_delete(c2Handle);
                    return err;
                }
                std::shared_ptr<MfxC2BufferQueueBlockPoolData> poolData =
                        std::make_shared<MfxC2BufferQueueBlockPoolData>(
                                slotBuffer->getGenerationNumber(),
                                mProducerId, slot,
                                shared_from_this());
                mPoolDatas[slot] = poolData;
                *block = _C2BlockFactory::CreateGraphicBlock(alloc, poolData);
                auto iter = gbuffer_.find(c2Handle);
                if (iter == gbuffer_.end()) {
                    gbuffer_.emplace(c2Handle, slotBuffer->handle);
                } else {
                    iter->second = slotBuffer->handle;
                }
                return C2_OK;
            }
            // Block was not created. call requestBuffer# again next time.
            slotBuffer.clear();
            (void)mProducer->cancelBuffer(slot, hFenceWrapper.getHandle()).isOk();
        }
        return C2_BAD_VALUE;
    }

public:
    Impl(const std::shared_ptr<C2Allocator> &allocator)
        : mInit(C2_OK), mProducerId(0), mGeneration(0),
          mDqFailure(0), mLastDqTs(0), mLastDqLogTs(0),
          mAllocator(allocator) {
    }

    ~Impl() {
        MFX_DEBUG_TRACE_FUNC;
        bool noInit = false;
        try {
                for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
                if (!noInit && mProducer) {
                    Return<HStatus> transResult =
                            mProducer->detachBuffer(static_cast<int32_t>(i));
                    noInit = !transResult.isOk() ||
                            static_cast<HStatus>(transResult) == HStatus::NO_INIT;
                }
                mBuffers[i].clear();
            }
            gbuffer_.clear();
        } catch(const std::exception& e) {
            MFX_DEBUG_TRACE_STREAM("Got an exception: " << e.what());
        }
    }

    c2_status_t handle(const C2Handle * c2_hdl, buffer_handle_t *hndl){
        MFX_DEBUG_TRACE_FUNC;
        auto iter = gbuffer_.find(c2_hdl);
        if (iter != gbuffer_.end()) {
            buffer_handle_t handle = iter->second;
            if(handle != NULL) {
                *hndl = handle;
                return C2_OK;
            }
        }
        MFX_DEBUG_TRACE_PRINTF("not BQ buffer" );
        return C2_CORRUPTED;
    }

    c2_status_t fetchGraphicBlock(
            uint32_t width,
            uint32_t height,
            uint32_t format,
            C2MemoryUsage usage,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {
        MFX_DEBUG_TRACE_FUNC;
        block->reset();
        if (mInit != C2_OK) {
            return mInit;
        }

        static int kMaxIgbpRetryDelayUs = 10000;

        std::unique_lock<std::mutex> lock(mMutex);
        if (mLastDqLogTs == 0) {
            mLastDqLogTs = getTimestampNow();
        } else {
            int64_t now = getTimestampNow();
            if (now >= mLastDqLogTs + 5000000) {
                if (now >= mLastDqTs + 1000000 || mDqFailure > 5) {
                    MFX_DEBUG_TRACE_PRINTF("last successful dequeue was %lld us ago, "
                          "%zu consecutive failures",
                          (long long)(now - mLastDqTs), mDqFailure);
                }
                mLastDqLogTs = now;
            }
        }
        if (mProducerId == 0) {
            std::shared_ptr<C2GraphicAllocation> alloc;
            c2_status_t err = mAllocator->newGraphicAllocation(
                    width, height, format, usage, &alloc);
            if (err != C2_OK) {
                return err;
            }
            std::shared_ptr<MfxC2BufferQueueBlockPoolData> poolData =
                    std::make_shared<MfxC2BufferQueueBlockPoolData>(
                        0, (uint64_t)0, ~0, shared_from_this());
            *block = _C2BlockFactory::CreateGraphicBlock(alloc, poolData);
            MFX_DEBUG_TRACE_PRINTF("no producer allocated a buffer successfully");

            return C2_OK;
        }
        c2_status_t status = fetchFromIgbp_l(width, height, format, usage, block);
        if (status == C2_BLOCKING) {
            lock.unlock();
            // in order not to drain cpu from component's spinning
            ::usleep(kMaxIgbpRetryDelayUs);
        }

        MFX_DEBUG_TRACE_I32(status);
        return status;
    }

    void setRenderCallback(const OnRenderCallback &renderCallback) {
        MFX_DEBUG_TRACE_FUNC;
        std::scoped_lock<std::mutex> lock(mMutex);
        mRenderCallback = renderCallback;
    }

    void configureProducer(const sp<HGraphicBufferProducer> &producer) {
        MFX_DEBUG_TRACE_FUNC;
        uint64_t producerId = 0;
        uint32_t generation = 0;
        bool haveGeneration = false;
        if (producer) {
            Return<uint64_t> transResult = producer->getUniqueId();
            if (!transResult.isOk()) {
                MFX_DEBUG_TRACE_PRINTF("configureProducer -- failed to connect to the producer");
                return;
            }
            producerId = static_cast<uint64_t>(transResult);
            // TODO: provide gneration number from parameter.
            haveGeneration = getGenerationNumber(producer, &generation);
            if (!haveGeneration) {
                MFX_DEBUG_TRACE_PRINTF("get generationNumber failed %llu",
                      (unsigned long long)producerId);
            }
        }
        int migrated = 0;
        // poolDatas dtor should not be called during lock is held.
        std::shared_ptr<MfxC2BufferQueueBlockPoolData>
                poolDatas[NUM_BUFFER_SLOTS];
        {
            sp<GraphicBuffer> buffers[NUM_BUFFER_SLOTS];
            std::scoped_lock<std::mutex> lock(mMutex);
            bool noInit = false;
            for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
                if (!noInit && mProducer) {
                    Return<HStatus> transResult =
                            mProducer->detachBuffer(static_cast<int32_t>(i));
                    noInit = !transResult.isOk() ||
                             static_cast<HStatus>(transResult) == HStatus::NO_INIT;
                }
            }
            int32_t oldGeneration = mGeneration;
            if (producer) {
                mProducer = producer;
                mProducerId = producerId;
                mGeneration = haveGeneration ? generation : 0;
            } else {
                mProducer = nullptr;
                mProducerId = 0;
                mGeneration = 0;
                MFX_DEBUG_TRACE_PRINTF("invalid producer producer(%d), generation(%d)",
                      (bool)producer, haveGeneration);
            }
            if (mProducer && haveGeneration) { // migrate buffers
                for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
                    std::shared_ptr<MfxC2BufferQueueBlockPoolData> data =
                            mPoolDatas[i].lock();
                    if (data) {
                        int slot = data->migrate(
                                mProducer, generation,
                                producerId, mBuffers, oldGeneration);
                        if (slot >= 0) {
                            buffers[slot] = mBuffers[i];
                            mBuffers[i] = NULL;
                            poolDatas[slot] = data;
                            ++migrated;
                        }
                    } else {
                        // free buffer migrate
                        if (mBuffers[i]) {
                            sp<GraphicBuffer> const& graphicBuffer = mBuffers[i];
                            graphicBuffer->setGenerationNumber(generation);

                            HBuffer hBuffer{};
                            uint32_t hGenerationNumber{};
                            if (!b2h(graphicBuffer, &hBuffer, &hGenerationNumber))
                            {
                                MFX_DEBUG_TRACE_PRINTF("I to O conversion failed");
                                continue;
                            }

                            bool converted{};
                            status_t bStatus{};
                            int slot_new;
                            int *outSlot = &slot_new;
                            Return<void> transResult =
                                producer->attachBuffer(hBuffer, hGenerationNumber,
                                                       [&converted, &bStatus, outSlot](
                                                           HStatus hStatus, int32_t hSlot, bool releaseAll) {
                                                           converted = h2b(hStatus, &bStatus);
                                                           *outSlot = static_cast<int>(hSlot);
                                                           if (converted && releaseAll && bStatus == android::OK)
                                                           {
                                                               bStatus = android::INVALID_OPERATION;
                                                           }
                                                       });
                            if (!transResult.isOk() || !converted || bStatus != android::OK)
                            {
                                MFX_DEBUG_TRACE_PRINTF("attach failed %d", static_cast<int>(bStatus));
                                continue;
                            }
                            MFX_DEBUG_TRACE_PRINTF("local migration from gen %u : %u slot %d : %d",
                                                   oldGeneration, generation, i, slot_new);

                            buffers[slot_new] = mBuffers[i];
                            producer->cancelBuffer(slot_new, hidl_handle{}).isOk();
                            ++migrated;
                        }
                    }
                }
            }
            for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
                mBuffers[i] = buffers[i];
                mPoolDatas[i] = poolDatas[i];
            }
        }
        if (producer && haveGeneration) {
            MFX_DEBUG_TRACE_PRINTF("local generation change %u , "
                  "bqId: %llu migrated buffers # %d",
                  generation, (unsigned long long)producerId, migrated);
        }
    }

    c2_status_t requestNewBufferSet(int32_t bufferCount) {
        MFX_DEBUG_TRACE_FUNC;

        if (bufferCount <= 0) {
            MFX_DEBUG_TRACE_PRINTF("Invalid requested buffer count = %d", bufferCount);
            return C2_BAD_VALUE;
        }

        std::lock_guard<std::mutex> lock(mMutex);
        if (!mProducer) {
            MFX_DEBUG_TRACE_PRINTF("No HGraphicBufferProducer is configured...");
            return C2_NO_INIT;
        }

        mProducer->setMaxDequeuedBufferCount(bufferCount);
        return C2_OK;
    }

    void getBufferQueueProducer(sp<HGraphicBufferProducer> &producer) {
        producer = mProducer;
    }

private:
    friend struct MfxC2BufferQueueBlockPoolData;

    void cancel(uint32_t generation, uint64_t igbp_id, int32_t igbp_slot) {
        bool cancelled = false;
        {
            MFX_DEBUG_TRACE_FUNC;
            try {
                std::scoped_lock<std::mutex> lock(mMutex);
                if (generation == mGeneration && igbp_id == mProducerId && mProducer) {
                    (void)mProducer->cancelBuffer(igbp_slot, hidl_handle{}).isOk();
                    cancelled = true;
                }
            } catch(const std::exception& e) {
                MFX_DEBUG_TRACE_STREAM("Got an exception: " << e.what());
            }
        }
    }

    c2_status_t mInit;
    uint64_t mProducerId;
    uint32_t mGeneration;
    OnRenderCallback mRenderCallback;

    size_t mDqFailure;
    int64_t mLastDqTs;
    int64_t mLastDqLogTs;

    const std::shared_ptr<C2Allocator> mAllocator;

    std::mutex mMutex;
    sp<HGraphicBufferProducer> mProducer;
    sp<HGraphicBufferProducer> mSavedProducer;

    sp<GraphicBuffer> mBuffers[NUM_BUFFER_SLOTS];
    std::weak_ptr<MfxC2BufferQueueBlockPoolData> mPoolDatas[NUM_BUFFER_SLOTS];
    std::map<const C2Handle *, buffer_handle_t> gbuffer_;
};

MfxC2BufferQueueBlockPoolData::MfxC2BufferQueueBlockPoolData(
        uint32_t generation, uint64_t bqId, int32_t bqSlot,
        const std::shared_ptr<int>& owner,
        const sp<HGraphicBufferProducer>& producer) :
        held(producer && bqId != 0), local(false),
        generation(generation), bqId(bqId), bqSlot(bqSlot),
        transfer(false), attach(false), display(false),
        owner(owner), igbp(producer),
        localPool() {
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2BufferQueueBlockPoolData::MfxC2BufferQueueBlockPoolData(
        uint32_t generation, uint64_t bqId, int32_t bqSlot,
        const std::shared_ptr<MfxC2BufferQueueBlockPool::Impl>& pool) :
        held(true), local(true),
        generation(generation), bqId(bqId), bqSlot(bqSlot),
        transfer(false), attach(false), display(false),
        igbp(pool ? pool->mProducer : nullptr),
        localPool(pool) {
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2BufferQueueBlockPoolData::~MfxC2BufferQueueBlockPoolData() {
    MFX_DEBUG_TRACE_FUNC;
    if (!held || bqId == 0) {
        return;
    }
    if (local) {
        if (localPool) {
            localPool->cancel(generation, bqId, bqSlot);
        }
    } else if (igbp && !owner.expired()) {
        (void)igbp->cancelBuffer(bqSlot, hidl_handle{}).isOk();
    }
}

int MfxC2BufferQueueBlockPoolData::migrate(
        const sp<HGraphicBufferProducer>& producer,
        uint32_t toGeneration, uint64_t toBqId,
        sp<GraphicBuffer> *buffers, uint32_t oldGeneration) {
    MFX_DEBUG_TRACE_FUNC;
    std::scoped_lock<std::mutex> l(lock);
    if (!held || bqId == 0) {
        MFX_DEBUG_TRACE_PRINTF("buffer is not owned");
        return -1;
    }
    if (!local || !localPool) {
        MFX_DEBUG_TRACE_PRINTF("pool is not local");
        return -1;
    }
    if (bqSlot < 0 || bqSlot >= NUM_BUFFER_SLOTS || !buffers[bqSlot]) {
        MFX_DEBUG_TRACE_PRINTF("slot is not in effect");
        return -1;
    }
    if (toGeneration == generation && bqId == toBqId) {
        MFX_DEBUG_TRACE_PRINTF("cannot migrate to same bufferqueue");
        return -1;
    }
    if (oldGeneration != generation) {
        MFX_DEBUG_TRACE_PRINTF("cannot migrate stale buffer");
    }
    if (transfer) {
        // either transferred or detached.
        MFX_DEBUG_TRACE_PRINTF("buffer is in transfer");
        return -1;
    }
    sp<GraphicBuffer> const& graphicBuffer = buffers[bqSlot];
    graphicBuffer->setGenerationNumber(toGeneration);

    HBuffer hBuffer{};
    uint32_t hGenerationNumber{};
    if (!b2h(graphicBuffer, &hBuffer, &hGenerationNumber)) {
        MFX_DEBUG_TRACE_PRINTF("I to O conversion failed");
        return -1;
    }

    bool converted{};
    status_t bStatus{};
    int slot;
    int *outSlot = &slot;
    Return<void> transResult =
            producer->attachBuffer(hBuffer, hGenerationNumber,
                    [&converted, &bStatus, outSlot](
                            HStatus hStatus, int32_t hSlot, bool releaseAll) {
                        converted = h2b(hStatus, &bStatus);
                        *outSlot = static_cast<int>(hSlot);
                        if (converted && releaseAll && bStatus == android::OK) {
                            bStatus = android::INVALID_OPERATION;
                        }
                    });
    if (!transResult.isOk() || !converted || bStatus != android::OK) {
        MFX_DEBUG_TRACE_PRINTF("attach failed %d", static_cast<int>(bStatus));
        return -1;
    }
    MFX_DEBUG_TRACE_PRINTF("local migration from gen %u : %u slot %d : %d",
          generation, toGeneration, bqSlot, slot);
    generation = toGeneration;
    bqId = toBqId;
    bqSlot = slot;
    return slot;
}
MfxC2BufferQueueBlockPool::MfxC2BufferQueueBlockPool(
        const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId)
        : mAllocator(allocator), local_id_(localId), impl_(new Impl(allocator)) {}

c2_status_t MfxC2BufferQueueBlockPool::fetchGraphicBlock(
        uint32_t width,
        uint32_t height,
        uint32_t format,
        C2MemoryUsage usage,
        std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {
    MFX_DEBUG_TRACE_FUNC;
    if (impl_) {
        return impl_->fetchGraphicBlock(width, height, format, usage, block);
    }
    return C2_CORRUPTED;
}

void MfxC2BufferQueueBlockPool::configureProducer(const sp<HGraphicBufferProducer> &producer) {
    MFX_DEBUG_TRACE_FUNC;
    ALOGE("MfxC2BufferQueueBlockPool::configureProducer, line:%d", __LINE__);
    if (impl_) {
        impl_->configureProducer(producer);
    }
}

void MfxC2BufferQueueBlockPool::setRenderCallback(const OnRenderCallback &renderCallback) {
    MFX_DEBUG_TRACE_FUNC;
    ALOGE("MfxC2BufferQueueBlockPool::setRenderCallback, line:%d", __LINE__);
    if (impl_) {
        impl_->setRenderCallback(renderCallback);
    }
}

c2_status_t MfxC2BufferQueueBlockPool::ImportHandle(const std::shared_ptr<C2GraphicBlock> block, buffer_handle_t *hndl){
    MFX_DEBUG_TRACE_FUNC;
    uint32_t width, height, format, stride, igbp_slot, generation;
    uint64_t usage, igbp_id;
    _UnwrapNativeCodec2GrallocMetadata(block->handle(), &width, &height,
                                                &format, &usage, &stride, &generation, &igbp_id,
                                                &igbp_slot);

    if (impl_ && igbp_slot < NUM_BUFFER_SLOTS) {
        return impl_->handle(block->handle(), hndl);
    }

    MFX_DEBUG_TRACE_PRINTF("invalid C2GraphicBlock, igbp_slot = %d", igbp_slot);
    return C2_CORRUPTED;
}
c2_status_t MfxC2BufferQueueBlockPool::requestNewBufferSet(int32_t bufferCount) {
    if (impl_) {
        return impl_->requestNewBufferSet(bufferCount);
    }
    return C2_NO_INIT;
}

bool MfxC2BufferQueueBlockPool::outputSurfaceSet(void) {
    if (impl_) {
        sp<HGraphicBufferProducer> producer;
        impl_->getBufferQueueProducer(producer);

        if (!producer) {
            ALOGI("No HGraphicBufferProducer is configured...");
            return false;
        }
    }
    return true;
}
