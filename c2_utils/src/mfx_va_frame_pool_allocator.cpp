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

#if defined(LIBVA_SUPPORT)

#include "mfx_va_frame_pool_allocator.h"
#include "mfx_c2_buffer_queue.h"

#include "mfx_c2_utils.h"
#include "mfx_debug.h"

#include <C2AllocatorGralloc.h>

using namespace android;

mfxStatus MfxVaFramePoolAllocator::AllocFrames(mfxFrameAllocRequest *request,
    mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;
    c2_status_t res = C2_OK;

    std::lock_guard<std::mutex> lock(mutex_);
    // The max buffer needed is calculated from c2 framework (CCodecBufferChannel.cpp):
    // output->maxDequeueBuffers = numOutputSlots + reorderDepth.value(0) + kRenderingDepth(3);
    // if (!secure) {
    //      output->maxDequeueBuffers += numInputSlots;
    // }
    // numInputSlots = inputDelayValue(input_delay_) + pipelineDelayValue(0) + kSmoothnessFactor(4);
    // numOutputSlots = outputDelayValue(output_delay_) + kSmoothnessFactor(4);
    // buffers_count = output_delay_ + kSmoothnessFactor(4) + kRenderingDepth(3) + input_delay_(2) + kSmoothnessFactor(4)
    int max_buffers = suggest_buffer_cnt_ + 13;

    if (request->Type & MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET) {

        do {
            if (!gralloc_allocator_) {
                res = MfxGrallocAllocator::Create(&gralloc_allocator_);
                if(C2_OK != res) {
                    mfx_res = MFX_ERR_NOT_INITIALIZED;
                    break;
                }
            }

            if (!c2_allocator_) {
                mfx_res = MFX_ERR_NOT_INITIALIZED;
                break;
            }
            if (request->Info.FourCC != MFX_FOURCC_NV12 &&
                request->Info.FourCC != MFX_FOURCC_P010) {
                mfx_res = MFX_ERR_UNSUPPORTED;
                break;
            }
            std::unique_ptr<mfxMemId[]> mids { new (std::nothrow)mfxMemId[max_buffers] };
            if (!mids) {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
                break;
            }

#ifdef MFX_BUFFER_QUEUE
            res = std::static_pointer_cast<MfxC2BufferQueueBlockPool>(c2_allocator_)->requestNewBufferSet(max_buffers);
            if(res != C2_OK) {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
                break;
            }
#endif

            response->NumFrameActual = 0;

            for (int i = 0; i < max_buffers; ++i) {

                std::shared_ptr<C2GraphicBlock> new_block;
                do{
                    res = c2_allocator_->fetchGraphicBlock(
                        request->Info.Width, request->Info.Height,
                        MfxFourCCToGralloc(request->Info.FourCC),
                        { C2AndroidMemoryUsage::CPU_READ|C2AndroidMemoryUsage::HW_COMPOSER_READ, C2AndroidMemoryUsage::HW_CODEC_WRITE },
                        &new_block);
                } while(res == C2_BLOCKING);

                uint64_t id;
                buffer_handle_t hndl = android::UnwrapNativeCodec2GrallocHandle(new_block->handle());
                gralloc_allocator_->GetBackingStore(hndl, &id);
                cached_buffer_id_.emplace(id, i);

                // deep copy to have unique_ptr as pool_ required unique_ptr
                std::unique_ptr<C2GraphicBlock> unique_block = std::make_unique<C2GraphicBlock>(*new_block);

                if (C2_OK != res) {
                    mfx_res = MFX_ERR_MEMORY_ALLOC;
                    break;
                }
                bool decode_target = true;
                mfx_res = ConvertGrallocToVa(hndl, decode_target, &mids[i]);
                if (MFX_ERR_NONE != mfx_res) break;

                MFX_DEBUG_TRACE_STREAM(NAMED(unique_block->handle()) << NAMED(mids[i]));

                pool_->Append(std::move(unique_block));//tmp cache it, in case return it to system and alloc again at once.

                ++response->NumFrameActual;
            }
            MFX_DEBUG_TRACE_I32(response->NumFrameActual);
            MFX_DEBUG_TRACE_I32(request->NumFrameMin);

            if (response->NumFrameActual >= request->NumFrameMin) {
                response->mids = mids.release();
                pool_ = std::make_unique<MfxPool<C2GraphicBlock>>(); //release graphic buffer
                mfx_res = MFX_ERR_NONE; // suppress the error if allocated enough
            } else {
                response->NumFrameActual = 0;
                response->mids = nullptr;
                // recreate pool_ to clean it
                FreeAllMappings();
                pool_ = std::make_unique<MfxPool<C2GraphicBlock>>();
            }
        } while(false);
    } else {
        mfx_res = AllocFrames(request, response);
    }

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
