// Copyright (c) 2017-2022 Intel Corporation
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
    mfx_frame->Info.CropW = width;
    mfx_frame->Info.CropH = height;
    mfx_frame->Info.CropX = 0;
    mfx_frame->Info.CropY = 0;
    mfx_frame->Info.FourCC = fourcc;

    mfx_frame->Data.TimeStamp = TimestampC2ToMfx(timestamp);

    mfx_frame->Data.FrameOrder = frame_index;
}

mfxStatus InitMfxFrameSW(
    uint64_t timestamp, uint64_t frame_index,
    uint8_t *data,
    uint32_t width, uint32_t height, uint32_t stride, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus res = MFX_ERR_NONE;
    mfxFrameInfo input_info = info;
    input_info.Width = width;
    input_info.Height = height;

    InitMfxFrameHeader(timestamp, frame_index, width, height, fourcc, info, mfx_frame);

    mfx_frame->Data.MemType = MFX_MEMTYPE_SYSTEM_MEMORY;
    mfx_frame->Data.Pitch = MFX_ALIGN_32(stride);

    res = MFXLoadSurfaceSW(data, stride, input_info, mfx_frame);

    return res;
}

mfxStatus MFXLoadSurfaceSW(uint8_t *data, uint32_t stride, const mfxFrameInfo& input_info, mfxFrameSurface1* srf)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus res = MFX_ERR_NONE;

    uint32_t nOWidth = input_info.Width;
    uint32_t nOHeight = input_info.Height;
    uint32_t nOPitch = stride;

    uint32_t nCropX = srf->Info.CropX;
    uint32_t nCropY = srf->Info.CropY;
    uint32_t nCropW = srf->Info.CropW;
    uint32_t nCropH = srf->Info.CropH;
    uint32_t nPitch = srf->Data.Pitch;

    if (!MFX_C2_IS_COPY_NEEDED(srf->Data.MemType, input_info, srf->Info)) {
        srf->Data.Y  = data;
        srf->Data.U = data + nOWidth * nOHeight;
        srf->Data.V = srf->Data.UV + 1;
        srf->Data.Pitch = nOPitch;
    } else {
        uint32_t i = 0;
        uint8_t* Y  = data;
        uint8_t* UV = data + nOPitch * nOHeight;

        // if input surface width or height is not 16bit aligned, do copy here
        for (i = 0; i < nCropH/2; ++i)
        {
            // copying Y
            uint8_t *src = Y + nCropX + (nCropY + i)*nOPitch;
            uint8_t *dst = srf->Data.Y + nCropX + (nCropY + i)*nPitch;
            std::copy(src, src + nCropW, dst);

            // copying UV
            src = UV + nCropX + (nCropY/2 + i)*nOPitch;
            dst = srf->Data.UV + nCropX + (nCropY/2 + i)*nPitch;
            std::copy(src, src + nCropW, dst);
        }
        for (i = nCropH/2; i < nCropH; ++i)
        {
            // copying Y (remained data)
            uint8_t *src = Y + nCropX + (nCropY + i)*nOPitch;
            uint8_t *dst = srf->Data.Y + nCropX + (nCropY + i)*nPitch;
            std::copy(src, src + nCropW, dst);
        }
    }

    return res;
}

mfxStatus InitMfxFrameHW(
    uint64_t timestamp, uint64_t frame_index,
    mfxMemId mem_id,
    uint32_t width, uint32_t height, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus res = MFX_ERR_NONE;

    InitMfxFrameHeader(timestamp, frame_index, width, height, fourcc, info, mfx_frame);

    MFX_ZERO_MEMORY(mfx_frame->Data);
    mfx_frame->Data.MemType = MFX_MEMTYPE_EXTERNAL_FRAME;
    mfx_frame->Data.MemId = mem_id;

    return res;
}

uint32_t MFXGetSurfaceSize(uint32_t FourCC, uint32_t width, uint32_t height)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxU32 nbytes = 0;

    switch (FourCC) {
#ifdef USE_ONEVPL
        case MFX_FOURCC_I420:
#endif
        case MFX_FOURCC_NV12:
            nbytes = width * height + (width >> 1) * (height >> 1) + (width >> 1) * (height >> 1);
            break;
        case MFX_FOURCC_I010:
        case MFX_FOURCC_P010:
            nbytes = width * height + (width >> 1) * (height >> 1) + (width >> 1) * (height >> 1);
            nbytes *= 2;
            break;
        case MFX_FOURCC_RGB4:
            nbytes = width * height * 4;
            break;
        default:
            break;
    }

    return nbytes;
}

uint32_t MFXGetFreeSurfaceIdx(mfxFrameSurface1 *SurfacesPool, uint32_t nPoolSize)
{
    MFX_DEBUG_TRACE_FUNC;

    for (uint32_t i = 0; i < nPoolSize; i++) {
        if (0 == SurfacesPool[i].Data.Locked)
            return i;
    }
    return MFX_ERR_NOT_FOUND;
}

mfxStatus MFXAllocSystemMemorySurfacePool(uint8_t **buf, mfxFrameSurface1 *surfpool, mfxFrameInfo frame_info, uint32_t surfnum)
{
    MFX_DEBUG_TRACE_FUNC;

    // initialize surface pool (I420, RGB4 format)
    if (!surfpool)
        return MFX_ERR_NULL_PTR;

    uint32_t surfaceSize = MFXGetSurfaceSize(frame_info.FourCC, frame_info.Width, frame_info.Height);
    if (!surfaceSize)
        return MFX_ERR_MEMORY_ALLOC;

    size_t framePoolBufSize = static_cast<size_t>(surfaceSize) * surfnum;
    *buf = reinterpret_cast<uint8_t *>(calloc(framePoolBufSize, 1));

    uint32_t surfW = 0;
    uint32_t surfH = frame_info.Height;

    if (frame_info.FourCC == MFX_FOURCC_RGB4) {
        surfW = frame_info.Width * 4;

        for (uint32_t i = 0; i < surfnum; i++) {
            uint32_t buf_offset    = i * surfaceSize;
            surfpool[i].Data.B     = *buf + buf_offset;
            surfpool[i].Data.G     = surfpool[i].Data.B + 1;
            surfpool[i].Data.R     = surfpool[i].Data.B + 2;
            surfpool[i].Data.A     = surfpool[i].Data.B + 3;
            surfpool[i].Data.Pitch = surfW;
        }
    } else {
        surfW = (frame_info.FourCC == MFX_FOURCC_P010) ? frame_info.Width * 2 : frame_info.Width;

        for (uint32_t i = 0; i < surfnum; i++) {
            uint32_t buf_offset    = i * surfaceSize;
            surfpool[i].Data.Y     = *buf + buf_offset;
            surfpool[i].Data.U     = *buf + buf_offset + (surfW * surfH);
            surfpool[i].Data.V     = surfpool[i].Data.U + ((surfW / 2) * (surfH / 2));
            surfpool[i].Data.Pitch = surfW;
        }
    }

    return MFX_ERR_NONE;
}

void MFXFreeSystemMemorySurfacePool(uint8_t *buf, mfxFrameSurface1 *surfpool)
{
    MFX_DEBUG_TRACE_FUNC;

    if (buf)
        free(buf);

    if (surfpool)
        free(surfpool);
}
