/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2021 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_frame_out.h"
#include "mfx_debug.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_debug.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_frame_out"

c2_status_t MfxC2FrameOut::Create(const std::shared_ptr<MfxFrameConverter>& frame_converter,
                               std::shared_ptr<C2GraphicBlock> block,
                               const mfxFrameInfo& info,
                               c2_nsecs_t timeout,
                               MfxC2FrameOut* wrapper, buffer_handle_t hndl)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if (nullptr == wrapper) {
            res = C2_BAD_VALUE;
            break;
        }

        if ( (info.Width && info.Width > block->width()) ||
             (info.Height && info.Height > block->height()) ) {
            res = C2_BAD_VALUE;
            break;
        }

        uint64_t timestamp = 0;
        uint64_t frame_index = 0;

        std::shared_ptr<mfxFrameSurface1> mfx_frame =
            std::shared_ptr<mfxFrameSurface1>(new (std::nothrow)mfxFrameSurface1());

        if (nullptr == mfx_frame) {
            res = C2_BAD_VALUE;
            break;
        }

        if (nullptr != frame_converter) {

            mfxMemId mem_id = nullptr;
            bool decode_target = true;

            mfxStatus mfx_sts = frame_converter->ConvertGrallocToVa(hndl,
                decode_target, &mem_id);
            if (MFX_ERR_NONE != mfx_sts) {
                res = MfxStatusToC2(mfx_sts);
                break;
            }

            InitMfxFrameHW(timestamp, frame_index,
                mem_id, block->width(), block->height(), MFX_FOURCC_NV12, info,
                mfx_frame.get());
        } else {
            std::unique_ptr<C2GraphicView> view;
            res = MapGraphicBlock(*block, timeout, &view);
            if(C2_OK != res) break;

            wrapper->c2_graphic_view_ = std::shared_ptr<C2GraphicView>(std::move(view));

            const uint32_t stride = wrapper->c2_graphic_view_->layout().planes[C2PlanarLayout::PLANE_Y].rowInc;
            InitMfxNV12FrameSW(timestamp, frame_index,
                wrapper->c2_graphic_view_->data(),
                block->width(), block->height(), stride, MFX_FOURCC_NV12, info,
                mfx_frame.get());
        }

        wrapper->c2_graphic_block_ = block;
        wrapper->mfx_surface_ = mfx_frame;

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

std::shared_ptr<C2GraphicBlock> MfxC2FrameOut::GetC2GraphicBlock() const
{
    MFX_DEBUG_TRACE_FUNC;
    return c2_graphic_block_;
}

std::shared_ptr<C2GraphicView> MfxC2FrameOut::GetC2GraphicView() const
{
    MFX_DEBUG_TRACE_FUNC;
    return c2_graphic_view_;
}

std::shared_ptr<mfxFrameSurface1> MfxC2FrameOut::GetMfxFrameSurface() const
{
    MFX_DEBUG_TRACE_FUNC;
    return mfx_surface_;
}
