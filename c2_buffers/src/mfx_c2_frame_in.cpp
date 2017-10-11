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

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_frame_in"

MfxC2FrameIn::~MfxC2FrameIn()
{
    MFX_DEBUG_TRACE_FUNC;

    if (frame_converter_ && mfx_frame_ && mfx_frame_->Data.MemId) {
        frame_converter_->FreeGrallocToVaMapping(mfx_frame_->Data.MemId);
    }
}

static void InitMfxNV12FrameHeader(
    uint64_t timestamp, uint64_t frame_index,
    uint32_t width, uint32_t height, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

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

    mfx_frame->Data.TimeStamp = timestamp * 90000 / MFX_SECOND_NS;
    mfx_frame->Data.FrameOrder = frame_index;
}

static void InitMfxNV12FrameSW(
    uint64_t timestamp, uint64_t frame_index,
    const uint8_t* data,
    uint32_t width, uint32_t height, mfxFrameSurface1* mfx_frame)
{
    memset(mfx_frame, 0, sizeof(mfxFrameSurface1));

    InitMfxNV12FrameHeader(timestamp, frame_index, width, height, mfx_frame);

    mfx_frame->Data.MemType = MFX_MEMTYPE_SYSTEM_MEMORY;

    uint32_t stride = width;

    mfx_frame->Data.PitchHigh = stride / (std::numeric_limits<mfxU16>::max() + 1ul);
    mfx_frame->Data.PitchLow = stride % (std::numeric_limits<mfxU16>::max() + 1ul);
    // TODO: 16-byte align requirement is not fulfilled - copy might be needed
    mfx_frame->Data.Y = const_cast<uint8_t*>(data);
    mfx_frame->Data.UV = mfx_frame->Data.Y + stride * height;
    mfx_frame->Data.V = mfx_frame->Data.UV;
}

static void InitMfxNV12FrameHW(
    uint64_t timestamp, uint64_t frame_index,
    mfxMemId mem_id,
    uint32_t width, uint32_t height, mfxFrameSurface1* mfx_frame)
{
    memset(mfx_frame, 0, sizeof(mfxFrameSurface1));

    InitMfxNV12FrameHeader(timestamp, frame_index, width, height, mfx_frame);

    mfx_frame->Data.MemType = MFX_MEMTYPE_EXTERNAL_FRAME;
    mfx_frame->Data.MemId = mem_id;
}

status_t MfxC2FrameIn::Create(MfxFrameConverter* frame_converter,
    C2BufferPack& buf_pack, nsecs_t timeout, MfxC2FrameIn* wrapper)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if (nullptr == wrapper) {
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<android::C2ConstGraphicBlock> c_graph_block;
        res = GetC2ConstGraphicBlock(buf_pack, &c_graph_block);
        if(C2_OK != res) break;

        std::unique_ptr<mfxFrameSurface1> unique_mfx_frame =
            std::make_unique<mfxFrameSurface1>();

        if (nullptr != c_graph_block->handle()) {
            if (nullptr == frame_converter) {
                res = C2_CORRUPTED;
                break;
            }

            mfxMemId mem_id = nullptr;
            bool decode_target = false;

            mfxStatus mfx_sts = frame_converter->ConvertGrallocToVa(c_graph_block->handle(),
                decode_target, &mem_id);
            if (MFX_ERR_NONE != mfx_sts) {
                res = MfxStatusToC2(mfx_sts);
                break;
            }

            InitMfxNV12FrameHW(buf_pack.ordinal.timestamp, buf_pack.ordinal.frame_index,
                mem_id, c_graph_block->width(), c_graph_block->height(),
                unique_mfx_frame.get());
        } else {
            const uint8_t* raw = nullptr;
            res = MapConstGraphicBlock(*c_graph_block, timeout, &raw);
            if(C2_OK != res) break;

            InitMfxNV12FrameSW(buf_pack.ordinal.timestamp, buf_pack.ordinal.frame_index,
                raw, c_graph_block->width(), c_graph_block->height(),
                unique_mfx_frame.get());
        }

        wrapper->frame_converter_ = frame_converter;
        wrapper->mfx_frame_ = std::move(unique_mfx_frame);
        wrapper->c2_buffer_ = std::move(buf_pack.buffers.front());

    } while(false);

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}
