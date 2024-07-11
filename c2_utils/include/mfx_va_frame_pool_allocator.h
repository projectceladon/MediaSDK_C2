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

#pragma once

#if defined(LIBVA_SUPPORT)

#include "mfx_va_allocator.h"
#include "mfx_frame_converter.h"
#include "mfx_frame_pool_allocator.h"
#include "mfx_pool.h"
#include <mutex>
#include "mfx_debug.h"

// Complex allocator: allocates pool through MfxFrameAllocator interface.
// Allocates for every required item of mfxAllocRequest pair:
// (C2GraphicBlock, mfxMemId).
// Then alloc method gives pre-allocated frames from the pool.
class MfxVaFramePoolAllocator : public MfxVaFrameAllocator, public MfxFramePoolAllocator
{
public:
    MfxVaFramePoolAllocator(VADisplay dpy):
        MfxVaFrameAllocator(dpy)
    {
        m_pool = std::make_unique<MfxPool<C2GraphicBlock>>();
    }
    virtual ~MfxVaFramePoolAllocator() = default;
private:
    virtual void SetC2Allocator(std::shared_ptr<C2BlockPool> c2_allocator) override
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_c2Allocator = std::move(c2_allocator);
    }

    virtual std::shared_ptr<C2GraphicBlock> Alloc() override
    {
        MFX_DEBUG_TRACE_FUNC;
        std::shared_ptr<C2GraphicBlock> res = m_pool->Alloc();
        MFX_DEBUG_TRACE_STREAM(res);
        return res;
    }

    // Forget about allocated resources.
    virtual void Reset() override
    {
        MFX_DEBUG_TRACE_FUNC;
        m_pool = std::make_unique<MfxPool<C2GraphicBlock>>();
        m_cachedBufferId.clear();
    }
    virtual void SetBufferCount(unsigned int cnt) override
    {
        MFX_DEBUG_TRACE_FUNC;
        m_uSuggestBufferCnt = cnt;
    }

    virtual void SetConsumerUsage(uint64_t usage) override
    {
        MFX_DEBUG_TRACE_FUNC;
        m_consumerUsage = usage;
    }

    bool InCache(uint64_t id) {
        auto it = m_cachedBufferId.find(id);
        if (it == m_cachedBufferId.end()){
            return false;
        }

        return true;
    }
private:
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) override;

    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response) override;

private:
    std::mutex m_mutex;

    std::shared_ptr<C2BlockPool> m_c2Allocator;

    std::unique_ptr<MfxPool<C2GraphicBlock>> m_pool;

    std::map<uint64_t, int> m_cachedBufferId;

    unsigned int m_uSuggestBufferCnt = 0;

    uint64_t m_consumerUsage = 0;

private:
    MFX_CLASS_NO_COPY(MfxVaFramePoolAllocator)
};

#endif //#if defined(LIBVA_SUPPORT)
