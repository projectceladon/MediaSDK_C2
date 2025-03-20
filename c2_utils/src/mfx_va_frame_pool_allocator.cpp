// Copyright (c) 2017-2022 Intel Corporation
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

#if defined(LIBVA_SUPPORT)

#include "mfx_va_frame_pool_allocator.h"
#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#include <C2AllocatorGralloc.h>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_va_pool_allocator"

const size_t kSmoothnessFactor = 4;
const size_t kRenderingDepth = 3;

mfxStatus MfxVaFramePoolAllocator::AllocFrames(mfxFrameAllocRequest *request,
    mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;
    c2_status_t res = C2_OK;

    std::lock_guard<std::mutex> lock(m_mutex);
    // The max buffer needed is calculated from c2 framework (CCodecBufferChannel.cpp):
    // output->maxDequeueBuffers = numOutputSlots + reorderDepth.value(0) + kRenderingDepth(3);
    // numOutputSlots = outputDelayValue(output_delay_) + kSmoothnessFactor(4);
    // buffers_count = output_delay_ + kSmoothnessFactor(4) + kRenderingDepth(3)
    int max_buffers = m_uSuggestBufferCnt + kSmoothnessFactor + kRenderingDepth;
    int min_buffers = MFX_MAX(request->NumFrameSuggested, MFX_MAX(request->NumFrameMin, 1));
    int opt_buffers = max_buffers; // optimal buffer count for better performance
    if (max_buffers < request->NumFrameMin) return MFX_ERR_MEMORY_ALLOC;

    // For 4K or 8K videos, limit buffer count to save memory
    if (IS_8K_VIDEO(request->Info.Width, request->Info.Height)) {
        opt_buffers = MFX_MAX(min_buffers, 4) + kSmoothnessFactor;
    } else if (IS_4K_VIDEO(request->Info.Width, request->Info.Height)) {
        opt_buffers = MFX_MAX(min_buffers, 4) + kSmoothnessFactor + kRenderingDepth;
    }
    MFX_DEBUG_TRACE_I32(max_buffers);
    MFX_DEBUG_TRACE_I32(opt_buffers);
    MFX_DEBUG_TRACE_I32(request->NumFrameMin);
    MFX_DEBUG_TRACE_I32(request->NumFrameSuggested);
    MFX_DEBUG_TRACE_I32(request->Info.Width);
    MFX_DEBUG_TRACE_I32(request->Info.Height);
    MFX_DEBUG_TRACE_I32(request->Info.CropW);
    MFX_DEBUG_TRACE_I32(request->Info.CropH);
    MFX_DEBUG_TRACE_I64(m_consumerUsage);

    if (request->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET) {

        do {
            if (!m_c2Allocator) {
                mfx_res = MFX_ERR_NOT_INITIALIZED;
                break;
            }
            if (request->Info.FourCC != MFX_FOURCC_NV12 &&
                request->Info.FourCC != MFX_FOURCC_P010) {
                mfx_res = MFX_ERR_UNSUPPORTED;
                break;
            }
            std::unique_ptr<mfxMemId[]> mids { new (std::nothrow)mfxMemId[opt_buffers] };
            if (!mids) {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
                break;
            }

            response->NumFrameActual = 0;

            response->NumFrameActual = opt_buffers;
            response->mids = mids.release();
        } while (false);
    } else {
        response->NumFrameActual = 0;
        response->mids = nullptr;
        mfx_res = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFramePoolAllocator::FreeFrames(mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    for (int i = 0; i < response->NumFrameActual; ++i) {
        FreeGrallocToVaMapping(response->mids[i]);
    }
    delete[] response->mids;

    return MFX_ERR_NONE;
}

#endif // #if defined(LIBVA_SUPPORT)
