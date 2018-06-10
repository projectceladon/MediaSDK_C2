/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_defs.h"
#include "mfx_c2_utils.h"
#include "mfx_debug.h"

mfxVersion g_required_mfx_version = { {MFX_VERSION_MINOR, MFX_VERSION_MAJOR} };

static void InitMfxNV12FrameHeader(
    uint64_t timestamp, uint64_t frame_index,
    uint32_t width, uint32_t height, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    mfx_frame->Info = info;//apply component's mfxFrameInfo
    mfx_frame->Info.Width = width;
    mfx_frame->Info.Height = height;
    mfx_frame->Info.CropX = 0;
    mfx_frame->Info.CropY = 0;
    mfx_frame->Info.CropW = width;
    mfx_frame->Info.CropH = height;

    mfx_frame->Data.TimeStamp = TimestampC2ToMfx(timestamp);

    mfx_frame->Data.FrameOrder = frame_index;
}

void InitMfxNV12FrameSW(
    uint64_t timestamp, uint64_t frame_index,
    const uint8_t *const *data,
    uint32_t width, uint32_t height, uint32_t stride, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    InitMfxNV12FrameHeader(timestamp, frame_index, width, height, info, mfx_frame);

    mfx_frame->Data.MemType = MFX_MEMTYPE_SYSTEM_MEMORY;

    mfx_frame->Data.PitchHigh = stride / (std::numeric_limits<mfxU16>::max() + 1ul);
    mfx_frame->Data.PitchLow = stride % (std::numeric_limits<mfxU16>::max() + 1ul);
    // TODO: 16-byte align requirement is not fulfilled - copy might be needed
    mfx_frame->Data.Y = const_cast<uint8_t*>(data[0]);
    mfx_frame->Data.UV = const_cast<uint8_t*>(data[1]);
    mfx_frame->Data.V = const_cast<uint8_t*>(data[2]);
}

void InitMfxNV12FrameHW(
    uint64_t timestamp, uint64_t frame_index,
    mfxMemId mem_id,
    uint32_t width, uint32_t height, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    InitMfxNV12FrameHeader(timestamp, frame_index, width, height, info, mfx_frame);

    mfx_frame->Data.MemType = MFX_MEMTYPE_EXTERNAL_FRAME;
    mfx_frame->Data.MemId = mem_id;
}
