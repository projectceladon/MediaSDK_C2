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

static void InitMfxNV12Frame(uint8_t* data, uint32_t width, uint32_t height, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    memset(mfx_frame, 0, sizeof(mfxFrameSurface1));

    uint32_t stride = width;

    mfx_frame->Info.BitDepthLuma = 8;
    mfx_frame->Info.BitDepthChroma = 8;
    mfx_frame->Info.FourCC = MFX_FOURCC_NV12;
    mfx_frame->Info.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    mfx_frame->Info.Width = width;
    mfx_frame->Info.Height = height;
    mfx_frame->Info.CropX = 0;
    mfx_frame->Info.CropY = 0;
    mfx_frame->Info.CropW = width;
    mfx_frame->Info.CropH = height;
    mfx_frame->Info.FrameRateExtN = 30;
    mfx_frame->Info.FrameRateExtD = 1;
    mfx_frame->Info.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;

    mfx_frame->Data.MemType = MFX_MEMTYPE_SYSTEM_MEMORY;
    mfx_frame->Data.PitchHigh = stride / (std::numeric_limits<mfxU16>::max() + 1ul);
    mfx_frame->Data.PitchLow = stride % (std::numeric_limits<mfxU16>::max() + 1ul);

    // TODO: 16-byte align requirement is not fulfilled - copy might be needed
    mfx_frame->Data.Y = data;
    mfx_frame->Data.UV = mfx_frame->Data.Y + stride * height;
    mfx_frame->Data.V = mfx_frame->Data.UV;
}

status_t MfxC2FrameOut::Create(std::shared_ptr<android::C2GraphicBlock> block,
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

        wrapper->mfx_surface_ = std::make_unique<mfxFrameSurface1>();
        wrapper->c2_graphic_block_ = block;

        uint8_t* raw = nullptr;
        res = MapGraphicBlock(*block, timeout, &raw);
        if(C2_OK != res) break;

        InitMfxNV12Frame(raw, block->width(), block->height(), wrapper->mfx_surface_.get());

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
}

std::unique_ptr<MfxC2FrameOut> MfxC2FrameOutPool::GetFrame()
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<MfxC2FrameOut> frame;
    if (!frame_pool_.empty()) {
        frame = std::move(frame_pool_.front());
        frame_pool_.pop_front();
    }

    return frame;
}

std::unique_ptr<MfxC2FrameOut> MfxC2FrameOutPool::GetFrameBySurface(mfxFrameSurface1* surface)
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

    return frame;
}
