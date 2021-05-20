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

#include <inttypes.h>
#include <map>
#include <memory>
#include <mutex>

#include <C2AllocatorGralloc.h>

#include "mfx_c2_buffer_queue.h"
#include "mfx_c2_allocator_id.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"

namespace android {

C2Allocator* createAllocator(C2Allocator::id_t allocatorId) {
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_PRINTF("%s(): Fallback to create C2AllocatorGralloc (id=%u)", __func__, allocatorId);
    return new C2AllocatorGralloc(allocatorId, true);
}

std::shared_ptr<C2Allocator> fetchAllocator(C2Allocator::id_t allocatorId) {
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_PRINTF("%s(allocatorId=%d)", __func__, allocatorId);
    static std::mutex sMutex;
    static std::map<C2Allocator::id_t, std::weak_ptr<C2Allocator>> sCacheAllocators;

    std::lock_guard<std::mutex> lock(sMutex);

    std::shared_ptr<C2Allocator> allocator;
    auto iter = sCacheAllocators.find(allocatorId);
    if (iter != sCacheAllocators.end()) {
        allocator = iter->second.lock();
        if (allocator != nullptr) {
            return allocator;
        }
    }

    allocator.reset(createAllocator(allocatorId));
    sCacheAllocators[allocatorId] = allocator;
    return allocator;
}

C2BlockPool* createBlockPool(C2Allocator::id_t allocatorId, C2BlockPool::local_id_t poolId) {
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_PRINTF("%s(allocatorId=%d, poolId=%" PRIu64 ")", __func__, allocatorId, poolId);

    std::shared_ptr<C2Allocator> allocator = fetchAllocator(allocatorId);
    if (allocator == nullptr) {
        MFX_DEBUG_TRACE_PRINTF("%s(): Failed to create allocator id=%u", __func__, allocatorId);
        return nullptr;
    }

    switch (allocatorId) {
    case MFX_BUFFERQUEUE:
        MFX_DEBUG_TRACE_PRINTF("%s(new MfxC2BufferQueueBlockPool allocatorId=%d, poolId=%" PRIu64 ")", __func__, allocatorId, poolId);
        return new MfxC2BufferQueueBlockPool(allocator, poolId);
    default:
        MFX_DEBUG_TRACE_PRINTF("%s(): Unknown allocator id=%u", __func__, allocatorId);
        return nullptr;
    }
}

}  // namespace android

extern "C" ::C2BlockPool* CreateBlockPool(::C2Allocator::id_t allocatorId,
                                          ::C2BlockPool::local_id_t poolId) {
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_PRINTF("%s(allocatorId=%d, poolId=%" PRIu64 ")", __func__, allocatorId, poolId);
    return ::android::createBlockPool(allocatorId, poolId);
}

extern "C" ::C2Allocator* CreateAllocator(::C2Allocator::id_t allocatorId, ::c2_status_t* status) {
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_PRINTF("%s(allocatorId=%d)", __func__, allocatorId);

    ::C2Allocator* res = ::android::createAllocator(allocatorId);
    *status = (res != nullptr) ? C2_OK : C2_BAD_INDEX;
    return res;
}
