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

#include "mfx_c2_frame_in.h"
#include "mfx_debug.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_debug.h"
#include <string.h>
#include <C2AllocatorGralloc.h>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_frame_in"

MfxC2FrameIn::~MfxC2FrameIn()
{
    MFX_DEBUG_TRACE_FUNC;
    if (m_frameConverter && m_pMfxFrameSurface && m_pMfxFrameSurface->Data.MemId) {
        m_frameConverter->FreeGrallocToVaMapping(m_pMfxFrameSurface->Data.MemId);
        delete m_pMfxFrameSurface;
        m_pMfxFrameSurface = nullptr;
    }
}

c2_status_t MfxC2FrameIn::init(std::shared_ptr<MfxFrameConverter> frame_converter,  std::unique_ptr<const C2GraphicView> c_graph_view,
        C2FrameData& buf_pack, mfxFrameSurface1 *mfx_frame)
{
    m_c2GraphicView = std::move(c_graph_view);
    m_frameConverter = std::move(frame_converter);
    m_pMfxFrameSurface = mfx_frame;
    m_c2Buffer = std::move(buf_pack.buffers.front());

    return C2_OK;
}

c2_status_t MfxC2FrameIn::init(std::shared_ptr<MfxFrameConverter> frame_converter,
    C2FrameData& buf_pack, const mfxFrameInfo& info, mfxFrameSurface1 *mfx_frame, c2_nsecs_t timeout)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    m_mfxFrameInfo = info;
    m_pMfxFrameSurface = mfx_frame;

    do {
        std::unique_ptr<C2ConstGraphicBlock> c_graph_block;
        res = GetC2ConstGraphicBlock(buf_pack, &c_graph_block);
        if(C2_OK != res) break;

        if ( (info.Width && info.Width > MFX_ALIGN_16(c_graph_block->width())) ||
             (info.Height && info.Height > MFX_ALIGN_16(c_graph_block->height())) ) {
            MFX_DEBUG_TRACE_I32(info.Width);
            MFX_DEBUG_TRACE_I32(info.Height);
            MFX_DEBUG_TRACE_I32(c_graph_block->width());
            MFX_DEBUG_TRACE_I32(c_graph_block->height());
            res = C2_BAD_VALUE;
            break;
        }

        m_frameConverter = frame_converter;
        if (nullptr != m_frameConverter) {
            res = MfxC2LoadSurfaceInHW(*c_graph_block, buf_pack);
        } else {
            res = MfxC2LoadSurfaceInSW(*c_graph_block, buf_pack, timeout);
        }

       if(C2_OK != res) break;

       m_c2Buffer = std::move(buf_pack.buffers.front());
    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2FrameIn::MfxC2LoadSurfaceInHW(C2ConstGraphicBlock& c_graph_block, C2FrameData& buf_pack)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    mfxMemId mem_id = nullptr;
    bool decode_target = false;

    auto hndl_deleter = [](native_handle_t *hndl) {
        native_handle_delete(hndl);
        hndl = nullptr;
    };

    std::unique_ptr<native_handle_t, decltype(hndl_deleter)> grallocHandle(
        android::UnwrapNativeCodec2GrallocHandle(c_graph_block.handle()), hndl_deleter);

    mfxStatus mfx_sts = m_frameConverter->ConvertGrallocToVa(grallocHandle.get(), decode_target, &mem_id);
    if (MFX_ERR_NONE != mfx_sts) {
        res = MfxStatusToC2(mfx_sts);
        return res;
    }

    InitMfxFrameHW(buf_pack.ordinal.timestamp.peeku(), buf_pack.ordinal.frameIndex.peeku(),
        mem_id, c_graph_block.width(), c_graph_block.height(), MFX_FOURCC_NV12, m_mfxFrameInfo,
        m_pMfxFrameSurface);

    return res;
}

c2_status_t MfxC2FrameIn::MfxC2LoadSurfaceInSW(C2ConstGraphicBlock& c_graph_block, C2FrameData& buf_pack, c2_nsecs_t timeout)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    res = MapConstGraphicBlock(c_graph_block, timeout, &m_c2GraphicView);
    if(C2_OK != res) {
        return res;
    }

    const uint8_t *pY = m_c2GraphicView->data()[C2PlanarLayout::PLANE_Y];
    const uint8_t *pU = m_c2GraphicView->data()[C2PlanarLayout::PLANE_U];
    const uint8_t *pV = m_c2GraphicView->data()[C2PlanarLayout::PLANE_V];

    uint32_t width = c_graph_block.width();
    uint32_t height = c_graph_block.height();
    uint32_t stride = m_c2GraphicView->layout().planes[C2PlanarLayout::PLANE_Y].rowInc;
    uint32_t y_plane_size = stride * height;
    MFX_DEBUG_TRACE_I32(width);
    MFX_DEBUG_TRACE_I32(height);
    MFX_DEBUG_TRACE_I32(stride);

    if (IsNV12(*m_c2GraphicView)) {
        mfx_sts = InitMfxFrameSW(buf_pack.ordinal.timestamp.peeku(), buf_pack.ordinal.frameIndex.peeku(),
            const_cast<uint8_t*>(m_c2GraphicView->data()[0]), const_cast<uint8_t*>(m_c2GraphicView->data()[1]),
            width, height, stride, MFX_FOURCC_NV12, m_mfxFrameInfo,
            m_pMfxFrameSurface);
        if (MFX_ERR_NONE != mfx_sts) {
            res = MfxStatusToC2(mfx_sts);
            return res;
        }
    } else if (IsP010(*m_c2GraphicView)) {
        mfx_sts = InitMfxFrameSW(buf_pack.ordinal.timestamp.peeku(), buf_pack.ordinal.frameIndex.peeku(),
            const_cast<uint8_t*>(m_c2GraphicView->data()[0]), const_cast<uint8_t*>(m_c2GraphicView->data()[1]),
            width, height, stride, MFX_FOURCC_P010, m_mfxFrameInfo,
            m_pMfxFrameSurface);
        if (MFX_ERR_NONE != mfx_sts) {
            res = MfxStatusToC2(mfx_sts);
            return res;
        }
    } else if (IsI420(*m_c2GraphicView) || IsYV12(*m_c2GraphicView)) {

        if (stride * height * 3 / 2 > WIDTH_4K * HEIGHT_4K * 3 / 2) {
            MFX_DEBUG_TRACE_PRINTF("not enough memory to complete operation");
            return C2_NO_MEMORY;
        }

        m_yuvData = std::shared_ptr<uint8_t>(new (std::nothrow)uint8_t[WIDTH_4K * HEIGHT_4K * 3 / 2], std::default_delete<uint8_t[]>());
        if(m_yuvData == nullptr)
        {
            MFX_DEBUG_TRACE_MSG("unsuccessful allocation");
            return C2_NO_MEMORY;
        }

        //IYUV or YV12 to NV12 conversion
        memcpy(m_yuvData.get(), pY, y_plane_size);

        for (int j = 0; j < height / 2; j++) {
            uint8_t *ptr = m_yuvData.get() + y_plane_size + j * stride;
            for (int i = 0; i < stride / 2; i++) {
                memcpy(&ptr[i * 2], &pU[j*stride/2 + i], 1);
                memcpy(&ptr[i * 2 + 1], &pV[j*stride/2 + i], 1);
            }
        }

#if MFX_DEBUG_DUMP_FRAME == MFX_DEBUG_YES
        static int frameIndex = 0;
        static YUVWriter writer("/data/local/tmp",std::vector<std::string>({}),"encoder_frame.log");
        writer.Write(m_yuvData.get(), stride, height, frameIndex++);
#endif

        mfx_sts = InitMfxFrameSW(buf_pack.ordinal.timestamp.peeku(), buf_pack.ordinal.frameIndex.peeku(),
                                m_yuvData.get(), m_yuvData.get()+y_plane_size, width, height, stride, MFX_FOURCC_NV12, m_mfxFrameInfo,
                                m_pMfxFrameSurface);
        if (MFX_ERR_NONE != mfx_sts) {
            res = MfxStatusToC2(mfx_sts);
            return res;
        }
   } else {
       MFX_DEBUG_TRACE_PRINTF("unsupported format");
       return C2_BAD_VALUE;
   }

   return res;
}
