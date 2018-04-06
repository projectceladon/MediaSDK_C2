/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#if defined(LIBVA_SUPPORT)

#include "mfx_va_frame_pool_allocator.h"

#include "mfx_c2_utils.h"
#include "mfx_debug.h"

using namespace android;

mfxStatus MfxVaFramePoolAllocator::AllocFrames(mfxFrameAllocRequest *request,
    mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;

    std::lock_guard<std::mutex> lock(mutex_);

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
            std::unique_ptr<mfxMemId[]> mids { new (std::nothrow)mfxMemId[request->NumFrameSuggested] };
            if (!mids) {
                res = MFX_ERR_MEMORY_ALLOC;
                break;
            }

            response->NumFrameActual = 0;

            for (int i = 0; i < request->NumFrameSuggested; ++i) {

                std::shared_ptr<C2GraphicBlock> new_block;

                c2_status_t err = c2_allocator_->fetchGraphicBlock(
                    request->Info.Width, request->Info.Height,
                    MfxFourCCToGralloc(request->Info.FourCC),
                    { C2MemoryUsage::CPU_READ, C2AndroidMemoryUsage::HW_CODEC_WRITE },
                    &new_block);

                if (C2_OK != err) {
                    res = MFX_ERR_MEMORY_ALLOC;
                    break;
                }
                bool decode_target = true;

                res = ConvertGrallocToVa(new_block->handle(), decode_target, &mids[i]);
                if (MFX_ERR_NONE != res) break;

                MFX_DEBUG_TRACE_STREAM(NAMED(new_block->handle()) << NAMED(mids[i]));

                pool_->Append(std::move(new_block));

                ++response->NumFrameActual;
            }

            if (response->NumFrameActual >= request->NumFrameMin) {
                response->mids = mids.release();
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
