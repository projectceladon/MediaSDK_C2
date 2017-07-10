/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_frame_in.h"
#include "mfx_debug.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_debug.h"

using namespace android;

static void InitMfxNV12Frame(
    uint64_t timestamp, uint64_t frame_index,
    const uint8_t* data, uint32_t width, uint32_t height, mfxFrameSurface1* mfx_frame)
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
    mfx_frame->Data.TimeStamp = timestamp * 90000 / MFX_SECOND_NS;
    mfx_frame->Data.FrameOrder = frame_index;

    // TODO: 16-byte align requirement is not fulfilled - copy might be needed
    mfx_frame->Data.Y = const_cast<uint8_t*>(data);
    mfx_frame->Data.UV = mfx_frame->Data.Y + stride * height;
    mfx_frame->Data.V = mfx_frame->Data.UV;
}

status_t MfxFrameWrapper::Create(
    C2BufferPack& buf_pack, nsecs_t timeout, MfxFrameWrapper* wrapper)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if (nullptr == wrapper) {
            res = C2_BAD_VALUE;
            break;
        }

        if(buf_pack.buffers.size() != 1) {
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<android::C2ConstGraphicBlock> c_graph_block;
        res = GetC2ConstGraphicBlock(buf_pack, &c_graph_block);
        if(C2_OK != res) break;

        const uint8_t* raw = nullptr;
        res = MapConstGraphicBlock(*c_graph_block, timeout, &raw);
        if(C2_OK != res) break;

        std::unique_ptr<mfxFrameSurface1> unique_mfx_frame =
            std::make_unique<mfxFrameSurface1>();

        InitMfxNV12Frame(buf_pack.ordinal.timestamp, buf_pack.ordinal.frame_index,
            raw, c_graph_block->width(), c_graph_block->height(),
            unique_mfx_frame.get());

        wrapper->mfx_frame_ = std::move(unique_mfx_frame);
        wrapper->c2_buffer_ = std::move(buf_pack.buffers.front());

    } while(false);

    MFX_DEBUG_TRACE_android_status_t(res);
    return res;
}
