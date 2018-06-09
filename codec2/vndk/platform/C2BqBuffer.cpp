/*
 * Copyright (C) 2018 The Android Open Source Project
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

//#define LOG_NDEBUG 0
#define LOG_TAG "C2BqBuffer"
#include <utils/Log.h>

#include <gui/BufferQueueDefs.h>
#include <list>
#include <map>
#include <mutex>

#include <C2AllocatorGralloc.h>
#include <C2BqBufferPriv.h>
#include <C2BlockInternal.h>

using ::android::AnwBuffer;
using ::android::BufferQueueDefs::NUM_BUFFER_SLOTS;
using ::android::C2AndroidMemoryUsage;
using ::android::Fence;
using ::android::GraphicBuffer;
using ::android::HGraphicBufferProducer;
using ::android::IGraphicBufferProducer;
using ::android::hidl_handle;
using ::android::sp;
using ::android::status_t;
using ::android::hardware::graphics::common::V1_0::PixelFormat;

class C2BufferQueueBlockPool::Impl {
private:
    c2_status_t fetchFromIgbp_l(
            uint32_t width,
            uint32_t height,
            uint32_t format,
            C2MemoryUsage usage,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {
        // We have an IGBP now.
        sp<Fence> fence = new Fence();
        C2AndroidMemoryUsage androidUsage = usage;
        status_t status;
        PixelFormat pixelFormat = static_cast<PixelFormat>(format);
        int slot;
        ALOGV("tries to dequeue buffer");
        mProducer->dequeueBuffer(
                width, height, pixelFormat, androidUsage.asGrallocUsage(), true,
                [&status, &slot, &fence](
                        int32_t tStatus, int32_t tSlot, hidl_handle const& tFence,
                        HGraphicBufferProducer::FrameEventHistoryDelta const& tTs) {
                    status = tStatus;
                    slot = tSlot;
                    if (!android::conversion::convertTo(fence.get(), tFence) &&
                            status == android::NO_ERROR) {
                        status = android::BAD_VALUE;
                    }
                    (void) tTs;
                });
        // dequeueBuffer returns flag.
        if (status < android::OK) {
            ALOGD("cannot dequeue buffer %d", status);
            if (status == android::INVALID_OPERATION) {
              // Too many buffer dequeued. retrying after some time is required.
              return C2_TIMED_OUT;
            } else {
              return C2_BAD_VALUE;
            }
        }
        ALOGV("dequeued a buffer successfully");
        native_handle_t* nh = nullptr;
        hidl_handle fenceHandle;
        if (fence) {
            android::conversion::wrapAs(&fenceHandle, &nh, *fence);
        }
        if (fence) {
            static constexpr int kFenceWaitTimeMs = 10;

            status = fence->wait(kFenceWaitTimeMs);
            if (status != android::NO_ERROR) {
                ALOGD("buffer fence wait error %d", status);
                mProducer->cancelBuffer(slot, fenceHandle);
                return C2_BAD_VALUE;
            }
        }

        sp<GraphicBuffer> &slotBuffer = mBuffers[slot];
        if (status & IGraphicBufferProducer::BUFFER_NEEDS_REALLOCATION || !slotBuffer) {
            if (!slotBuffer) {
                slotBuffer = new GraphicBuffer();
            }
            // N.B. This assumes requestBuffer# returns an existing allocation
            // instead of a new allocation.
            mProducer->requestBuffer(
                    slot,
                    [&status, &slotBuffer](int32_t tStatus, AnwBuffer const& tBuffer){
                        status = tStatus;
                        if (!android::conversion::convertTo(slotBuffer.get(), tBuffer) &&
                                status == android::NO_ERROR) {
                            status = android::BAD_VALUE;
                        }
                    });

            if (status != android::NO_ERROR) {
                mBuffers[slot].clear();
                mProducer->cancelBuffer(slot, fenceHandle);
                return C2_BAD_VALUE;
            }
        }
        if (mBuffers[slot]) {
            native_handle_t *grallocHandle = native_handle_clone(mBuffers[slot]->handle);

            if (grallocHandle) {
                ALOGV("buffer wraps %llu %d", (unsigned long long)mProducerId, slot);
                C2Handle *c2Handle = android::WrapNativeCodec2GrallocHandle(
                        grallocHandle,
                        mBuffers[slot]->width,
                        mBuffers[slot]->height,
                        mBuffers[slot]->format,
                        mBuffers[slot]->usage,
                        mBuffers[slot]->stride,
                        mProducerId, slot);
                if (c2Handle) {
                    std::shared_ptr<C2GraphicAllocation> alloc;
                    c2_status_t err = mAllocator->priorGraphicAllocation(c2Handle, &alloc);
                    if (err != C2_OK) {
                        return err;
                    }
                    *block = _C2BlockFactory::CreateGraphicBlock(alloc);
                    return C2_OK;
                }
            }
            // Block was not created. call requestBuffer# again next time.
            mBuffers[slot].clear();
            mProducer->cancelBuffer(slot, fenceHandle);
        }
        return C2_BAD_VALUE;
    }

public:
    Impl(const std::shared_ptr<C2Allocator> &allocator)
        : mInit(C2_OK), mProducerId(0), mAllocator(allocator) {
    }

    ~Impl() {}

    c2_status_t fetchGraphicBlock(
            uint32_t width,
            uint32_t height,
            uint32_t format,
            C2MemoryUsage usage,
            std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {
        block->reset();
        if (mInit != C2_OK) {
            return mInit;
        }

        static int kMaxIgbpRetry = 20; // TODO: small number can cause crash in releasing.
        static int kMaxIgbpRetryDelayUs = 10000;

        int curTry = 0;

        while (curTry++ < kMaxIgbpRetry) {
            std::unique_lock<std::mutex> lock(mMutex);
            // TODO: return C2_NO_INIT
            if (mProducerId == 0) {
                std::shared_ptr<C2GraphicAllocation> alloc;
                c2_status_t err = mAllocator->newGraphicAllocation(
                        width, height, format, usage, &alloc);
                if (err != C2_OK) {
                    return err;
                }
                *block = _C2BlockFactory::CreateGraphicBlock(alloc);
                ALOGV("allocated a buffer successfully");

                return C2_OK;
            }
            c2_status_t status = fetchFromIgbp_l(width, height, format, usage, block);
            if (status == C2_TIMED_OUT) {
                lock.unlock();
                ::usleep(kMaxIgbpRetryDelayUs);
                continue;
            }
            return status;
        }
        return C2_TIMED_OUT;
    }

    void configureProducer(const sp<HGraphicBufferProducer> &producer) {
        std::lock_guard<std::mutex> lock(mMutex);
        mProducer = producer;
        if (producer) {
            int32_t status;
            uint64_t &producerId = mProducerId;
            producer->getUniqueId(
                    [&status, &producerId](int32_t tStatus, int64_t tProducerId) {
                status = tStatus;
                producerId = tProducerId;
                    });
            if (status != android::OK) {
                mProducer = nullptr;
                mProducerId = 0;
            }

        } else {
            mProducerId = 0;
        }
        for (int i = 0; i < NUM_BUFFER_SLOTS; ++i) {
            mBuffers[i].clear();
        }
    }
private:
    c2_status_t mInit;
    uint64_t mProducerId;
    const std::shared_ptr<C2Allocator> mAllocator;

    std::mutex mMutex;
    sp<HGraphicBufferProducer> mProducer;

    sp<GraphicBuffer> mBuffers[NUM_BUFFER_SLOTS];
};

C2BufferQueueBlockPool::C2BufferQueueBlockPool(
        const std::shared_ptr<C2Allocator> &allocator, const local_id_t localId)
        : mAllocator(allocator), mLocalId(localId), mImpl(new Impl(allocator)) {}

C2BufferQueueBlockPool::~C2BufferQueueBlockPool() {}

c2_status_t C2BufferQueueBlockPool::fetchGraphicBlock(
        uint32_t width,
        uint32_t height,
        uint32_t format,
        C2MemoryUsage usage,
        std::shared_ptr<C2GraphicBlock> *block /* nonnull */) {
    if (mImpl) {
        return mImpl->fetchGraphicBlock(width, height, format, usage, block);
    }
    return C2_CORRUPTED;
}

void C2BufferQueueBlockPool::configureProducer(const sp<HGraphicBufferProducer> &producer) {
    if (mImpl) {
        mImpl->configureProducer(producer);
    }
}
