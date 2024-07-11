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
                               MfxC2FrameOut* wrapper, buffer_handle_t hndl)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if (nullptr == wrapper) {
            res = C2_BAD_VALUE;
            break;
        }

        if ( (info.Width && info.Width > MFX_ALIGN_16(block->width())) ||
             (info.Height && info.Height > MFX_ALIGN_16(block->height())) ) {
            MFX_DEBUG_TRACE_I32(info.Width);
            MFX_DEBUG_TRACE_I32(info.Height);
            MFX_DEBUG_TRACE_I32(block->width());
            MFX_DEBUG_TRACE_I32(block->height());
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
                mem_id, block->width(), block->height(), info.FourCC, info,
                mfx_frame.get());
        }

        wrapper->m_c2GraphicBlock = block;
        wrapper->m_mfxSurface = std::move(mfx_frame);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2FrameOut::Create(std::shared_ptr<C2GraphicBlock> block,
                               const mfxFrameInfo& info,
                               c2_nsecs_t timeout,
                               MfxC2FrameOut* wrapper)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if (nullptr == wrapper) {
            res = C2_BAD_VALUE;
            break;
        }

        if ( (info.Width && info.Width > MFX_ALIGN_16(block->width())) ||
             (info.Height && info.Height > MFX_ALIGN_16(block->height())) ) {
            MFX_DEBUG_TRACE_I32(info.Width);
            MFX_DEBUG_TRACE_I32(info.Height);
            MFX_DEBUG_TRACE_I32(block->width());
            MFX_DEBUG_TRACE_I32(block->height());
            res = C2_BAD_VALUE;
            break;
        }

        std::shared_ptr<mfxFrameSurface1> mfx_frame =
            std::shared_ptr<mfxFrameSurface1>(new (std::nothrow)mfxFrameSurface1());
        if (nullptr == mfx_frame) {
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2GraphicView> view;
        res = MapGraphicBlock(*block, timeout, &view);
        if(C2_OK != res) break;

        wrapper->m_c2GraphicView = std::shared_ptr<C2GraphicView>(std::move(view));

        uint64_t timestamp = 0;
        uint64_t frame_index = 0;
        const uint32_t stride = wrapper->m_c2GraphicView->layout().planes[C2PlanarLayout::PLANE_Y].rowInc;
        InitMfxFrameSW(timestamp, frame_index,
            const_cast<uint8_t*>(wrapper->m_c2GraphicView->data()[0]),
            const_cast<uint8_t*>(wrapper->m_c2GraphicView->data()[1]),
            block->width(), block->height(), stride, info.FourCC, info,
            mfx_frame.get());

        wrapper->m_c2GraphicBlock = block;
        wrapper->m_mfxSurface = std::move(mfx_frame);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

std::shared_ptr<C2GraphicBlock> MfxC2FrameOut::GetC2GraphicBlock() const
{
    MFX_DEBUG_TRACE_FUNC;
    return m_c2GraphicBlock;
}

std::shared_ptr<C2GraphicView> MfxC2FrameOut::GetC2GraphicView() const
{
    MFX_DEBUG_TRACE_FUNC;
    return m_c2GraphicView;
}

std::shared_ptr<mfxFrameSurface1> MfxC2FrameOut::GetMfxFrameSurface() const
{
    MFX_DEBUG_TRACE_FUNC;
    return m_mfxSurface;
}
