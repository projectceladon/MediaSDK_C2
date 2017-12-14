/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_frame_out.h"
#include "mfx_debug.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_debug.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_frame_out"

status_t MfxC2FrameOut::Create(MfxFrameConverter* frame_converter,
                               std::shared_ptr<android::C2GraphicBlock> block,
                               nsecs_t timeout,
                               std::unique_ptr<MfxC2FrameOut>& wrapper)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if (nullptr == wrapper) {
            res = C2_BAD_VALUE;
            break;
        }

        uint64_t timestamp = 0;
        uint64_t frame_index = 0;

        std::unique_ptr<mfxFrameSurface1> unique_mfx_frame =
            std::make_unique<mfxFrameSurface1>();

        if (nullptr != block->handle()) {
            if (nullptr == frame_converter) {
                res = C2_CORRUPTED;
                break;
            }

            mfxMemId mem_id = nullptr;
            bool decode_target = true;

            mfxStatus mfx_sts = frame_converter->ConvertGrallocToVa(block->handle(),
                decode_target, &mem_id);
            if (MFX_ERR_NONE != mfx_sts) {
                res = MfxStatusToC2(mfx_sts);
                break;
            }

            InitMfxNV12FrameHW(timestamp, frame_index,
                mem_id, block->width(), block->height(),
                unique_mfx_frame.get());
        } else {
            std::unique_ptr<C2GraphicView> graph_view;
            res = MapGraphicBlock(*block, timeout, &graph_view);
            if(C2_OK != res) break;

            InitMfxNV12FrameSW(timestamp, frame_index,
                graph_view->data(), block->width(), block->height(),
                unique_mfx_frame.get());
        }

        wrapper->c2_graphic_block_ = block;
        wrapper->frame_converter_ = frame_converter;
        wrapper->mfx_surface_ = std::move(unique_mfx_frame);

    } while(false);

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}

std::unique_ptr<android::C2Work> MfxC2FrameOut::GetC2Work()
{
    MFX_DEBUG_TRACE_FUNC;
    return std::move(work_);
}

std::shared_ptr<android::C2GraphicBlock> MfxC2FrameOut::GetC2GraphicBlock() const
{
    MFX_DEBUG_TRACE_FUNC;
    return c2_graphic_block_;
}

mfxFrameSurface1* MfxC2FrameOut::GetMfxFrameSurface() const
{
    MFX_DEBUG_TRACE_FUNC;
    return mfx_surface_.get();
}

void MfxC2FrameOut::PutC2Work(std::unique_ptr<android::C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;
    work_ = std::move(work);
}

void MfxC2FrameOutPool::AddFrame(std::unique_ptr<MfxC2FrameOut>&& frame)
{
    MFX_DEBUG_TRACE_FUNC;
    frame_pool_.push_back(std::move(frame));
    MFX_DEBUG_TRACE_I32(frame_pool_.size());
}

std::unique_ptr<MfxC2FrameOut> MfxC2FrameOutPool::AcquireUnlockedFrame()
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<MfxC2FrameOut> frame;
    std::list<std::unique_ptr<MfxC2FrameOut>>::iterator it = std::find_if (
        frame_pool_.begin(),
        frame_pool_.end(),
        [] (std::unique_ptr<MfxC2FrameOut>& p) { return !p->GetMfxFrameSurface()->Data.Locked; } );

    if (it != frame_pool_.end()) {
        frame = std::move((*it));
        frame_pool_.erase(it);
    }

    MFX_DEBUG_TRACE_I32(frame_pool_.size());
    MFX_DEBUG_TRACE_P(frame.get());

    return frame;
}

std::unique_ptr<MfxC2FrameOut> MfxC2FrameOutPool::AcquireFrameBySurface(mfxFrameSurface1* surface)
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<MfxC2FrameOut> frame;
    std::list<std::unique_ptr<MfxC2FrameOut>>::iterator it = std::find_if (
        frame_pool_.begin(),
        frame_pool_.end(),
        [&surface] (std::unique_ptr<MfxC2FrameOut>& p) { return p->GetMfxFrameSurface() == surface; } );

    if (it != frame_pool_.end()) {
        frame = std::move((*it));
        frame_pool_.erase(it);
    }

    MFX_DEBUG_TRACE_I32(frame_pool_.size());
    MFX_DEBUG_TRACE_P(frame.get());

    return frame;
}
