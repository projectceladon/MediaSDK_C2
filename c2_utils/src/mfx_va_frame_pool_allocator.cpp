/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#if defined(LIBVA_SUPPORT)

#include "mfx_va_frame_pool_allocator.h"
#include "mfx_c2_buffer_queue.h"

#include "mfx_c2_utils.h"
#include "mfx_debug.h"

using namespace android;

mfxStatus MfxVaFramePoolAllocator::AllocFrames(mfxFrameAllocRequest *request,
    mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;
    c2_status_t err;

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
            if (!c2_allocator_) {
                res = MFX_ERR_NOT_INITIALIZED;
                break;
            }
            if (request->Info.FourCC != MFX_FOURCC_NV12) {
                res = MFX_ERR_UNSUPPORTED;
                break;
            }
            std::unique_ptr<mfxMemId[]> mids { new (std::nothrow)mfxMemId[max_buffers] };
            if (!mids) {
                res = MFX_ERR_MEMORY_ALLOC;
                break;
            }
            err = std::static_pointer_cast<MfxC2BufferQueueBlockPool>(c2_allocator_)->requestNewBufferSet(max_buffers);
            if(err != C2_OK) {
                res = MFX_ERR_MEMORY_ALLOC;
                break;
            }

            response->NumFrameActual = 0;

            for (int i = 0; i < max_buffers; ++i) {

                std::shared_ptr<C2GraphicBlock> new_block;
                c2_status_t err;
                do{
                    err = c2_allocator_->fetchGraphicBlock(
                        request->Info.Width, request->Info.Height,
                        MfxFourCCToGralloc(request->Info.FourCC),
                        { C2AndroidMemoryUsage::CPU_READ|C2AndroidMemoryUsage::HW_COMPOSER_READ, C2AndroidMemoryUsage::HW_CODEC_WRITE },
                        &new_block);
                } while(err == C2_BLOCKING);

                buffer_handle_t hndl;
                std::static_pointer_cast<MfxC2BufferQueueBlockPool>(c2_allocator_)->ImportHandle(new_block, &hndl);
                cached_buffer_handle_.emplace(hndl, i);
                // deep copy to have unique_ptr as pool_ required unique_ptr
                std::unique_ptr<C2GraphicBlock> unique_block = std::make_unique<C2GraphicBlock>(*new_block);

                if (C2_OK != err) {
                    res = MFX_ERR_MEMORY_ALLOC;
                    break;
                }
                bool decode_target = true;
                res = ConvertGrallocToVa(hndl, decode_target, &mids[i]);
                if (MFX_ERR_NONE != res) break;

                MFX_DEBUG_TRACE_STREAM(NAMED(unique_block->handle()) << NAMED(mids[i]));

                pool_->Append(std::move(unique_block));//tmp cache it, in case return it to system and alloc again at once.

                ++response->NumFrameActual;
            }
            MFX_DEBUG_TRACE_I32(response->NumFrameActual);
            MFX_DEBUG_TRACE_I32(request->NumFrameMin);

            if (response->NumFrameActual >= request->NumFrameMin) {
                response->mids = mids.release();
                pool_ = std::make_unique<MfxPool<C2GraphicBlock>>(); //release graphic buffer
                res = MFX_ERR_NONE; // suppress the error if allocated enough
            } else {
                response->NumFrameActual = 0;
                response->mids = nullptr;
                // recreate pool_ to clean it
                FreeAllMappings();
                pool_ = std::make_unique<MfxPool<C2GraphicBlock>>();
            }
        } while(false);
    } else {
        res = AllocFrames(request, response);
    }

    return res;
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
