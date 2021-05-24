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

#include "mfx_defs.h"
#include "mfx_c2_utils.h"
#include "mfx_debug.h"

mfxVersion g_required_mfx_version = { {MFX_VERSION_MINOR, MFX_VERSION_MAJOR} };

static void InitMfxFrameHeader(
    uint64_t timestamp, uint64_t frame_index,
    uint32_t width, uint32_t height, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    mfx_frame->Info = info;//apply component's mfxFrameInfo
    mfx_frame->Info.Width = width;
    mfx_frame->Info.Height = height;
    mfx_frame->Info.CropX = 0;
    mfx_frame->Info.CropY = 0;
    mfx_frame->Info.CropW = width;
    mfx_frame->Info.CropH = height;
    mfx_frame->Info.FourCC = fourcc;

    mfx_frame->Data.TimeStamp = TimestampC2ToMfx(timestamp);

    mfx_frame->Data.FrameOrder = frame_index;
}

void InitMfxNV12FrameSW(
    uint64_t timestamp, uint64_t frame_index,
    const uint8_t *const *data,
    uint32_t width, uint32_t height, uint32_t stride, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    InitMfxFrameHeader(timestamp, frame_index, width, height, fourcc, info, mfx_frame);

    mfx_frame->Data.MemType = MFX_MEMTYPE_SYSTEM_MEMORY;

    mfx_frame->Data.PitchHigh = stride / (std::numeric_limits<mfxU16>::max() + 1ul);
    mfx_frame->Data.PitchLow = stride % (std::numeric_limits<mfxU16>::max() + 1ul);
    // TODO: 16-byte align requirement is not fulfilled - copy might be needed
    mfx_frame->Data.Y = const_cast<uint8_t*>(data[0]);
    mfx_frame->Data.UV = const_cast<uint8_t*>(data[1]);
    mfx_frame->Data.V = const_cast<uint8_t*>(data[2]);
}

void InitMfxFrameHW(
    uint64_t timestamp, uint64_t frame_index,
    mfxMemId mem_id,
    uint32_t width, uint32_t height, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    InitMfxFrameHeader(timestamp, frame_index, width, height, fourcc, info, mfx_frame);

    MFX_ZERO_MEMORY(mfx_frame->Data);
    mfx_frame->Data.MemType = MFX_MEMTYPE_EXTERNAL_FRAME;
    mfx_frame->Data.MemId = mem_id;
}
