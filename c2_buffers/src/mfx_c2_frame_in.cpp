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
    if (frame_converter_ && mfx_frame_surface_ && mfx_frame_surface_->Data.MemId) {
        frame_converter_->FreeGrallocToVaMapping(mfx_frame_surface_->Data.MemId);
        delete mfx_frame_surface_;
        mfx_frame_surface_ = nullptr;
    }
}

c2_status_t MfxC2FrameIn::init(std::shared_ptr<MfxFrameConverter> frame_converter,  std::unique_ptr<const C2GraphicView> c_graph_view,
        C2FrameData& buf_pack, mfxFrameSurface1 *mfx_frame)
{
    c2_graphic_view_ = std::move(c_graph_view);
    frame_converter_ = frame_converter;
    mfx_frame_surface_ = mfx_frame;
    c2_buffer_ = std::move(buf_pack.buffers.front());

    return C2_OK;
}

c2_status_t MfxC2FrameIn::init(std::shared_ptr<MfxFrameConverter> frame_converter,
    C2FrameData& buf_pack, const mfxFrameInfo& info, c2_nsecs_t timeout)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    do {
        std::unique_ptr<C2ConstGraphicBlock> c_graph_block;
        res = GetC2ConstGraphicBlock(buf_pack, &c_graph_block);
        if(C2_OK != res) break;

        if ( (info.Width && info.Width > c_graph_block->width()) ||
             (info.Height && info.Height > c_graph_block->height()) ) {
            res = C2_BAD_VALUE;
            break;
        }

        mfxFrameSurface1 *mfx_frame = new mfxFrameSurface1;
        MFX_ZERO_MEMORY(*mfx_frame);

        if (nullptr != frame_converter) {

            mfxMemId mem_id = nullptr;
            bool decode_target = false;
            native_handle_t *grallocHandle = android::UnwrapNativeCodec2GrallocHandle(c_graph_block->handle());

            mfxStatus mfx_sts = frame_converter->ConvertGrallocToVa(grallocHandle, decode_target, &mem_id);
            if (MFX_ERR_NONE != mfx_sts) {
                res = MfxStatusToC2(mfx_sts);
                break;
            }

            InitMfxFrameHW(buf_pack.ordinal.timestamp.peeku(), buf_pack.ordinal.frameIndex.peeku(),
                mem_id, c_graph_block->width(), c_graph_block->height(), MFX_FOURCC_NV12, info,
                mfx_frame);
        } else {
            res = MapConstGraphicBlock(*c_graph_block, timeout, &c2_graphic_view_);
            if(C2_OK != res) break;

            const uint8_t *pY = c2_graphic_view_->data()[C2PlanarLayout::PLANE_Y];
            const uint8_t *pU = c2_graphic_view_->data()[C2PlanarLayout::PLANE_U];
            const uint8_t *pV = c2_graphic_view_->data()[C2PlanarLayout::PLANE_V];

            uint32_t width = c_graph_block->width();
            uint32_t height = c_graph_block->height();
            uint32_t stride = c2_graphic_view_->layout().planes[C2PlanarLayout::PLANE_Y].rowInc;
            uint32_t y_plane_size = stride * height;

            if (IsNV12(*c2_graphic_view_)) {

                InitMfxFrameSW(buf_pack.ordinal.timestamp.peeku(), buf_pack.ordinal.frameIndex.peeku(),
                    c2_graphic_view_->data(),
                    width, height, stride, MFX_FOURCC_NV12, info,
                    mfx_frame);
            } else if (IsI420(*c2_graphic_view_) || IsYV12(*c2_graphic_view_)) {

                if (stride * height * 3 / 2 > WIDTH_4K * HEIGHT_4K * 3 / 2) {
                    MFX_DEBUG_TRACE_PRINTF("not enough memory to complete operation");
                    res = C2_NO_MEMORY;
                    break;
                }

                try
                {
                    yuv_data_ = std::shared_ptr<uint8_t>(new uint8_t[WIDTH_4K * HEIGHT_4K * 3 / 2], std::default_delete<uint8_t[]>());
                }
                catch(const std::exception&)
                {
                    MFX_DEBUG_TRACE_MSG("unsuccessful allocation");
                    res = C2_NO_MEMORY;
                    return res;
                }

                //IYUV or YV12 to NV12 conversion
                memcpy(yuv_data_.get(), pY, y_plane_size);

                for (int j = 0; j < height / 2; j++) {
                    uint8_t *ptr = yuv_data_.get() + y_plane_size + j * stride;
                    for (int i = 0; i < stride / 2; i++) {
                        memcpy(&ptr[i * 2], &pU[j*stride/2 + i], 1);
                        memcpy(&ptr[i * 2 + 1], &pV[j*stride/2 + i], 1);
                    }
                }

#if MFX_DEBUG_DUMP_FRAME == MFX_DEBUG_YES
                static int frameIndex = 0;
                static YUVWriter writer("/data/local/tmp",std::vector<std::string>({}),"encoder_frame.log");
                writer.Write(yuv_data_.get(), stride, height, frameIndex++);
#endif

                InitMfxFrameSW(buf_pack.ordinal.timestamp.peeku(), buf_pack.ordinal.frameIndex.peeku(),
                                        yuv_data_.get(), width, height, stride, MFX_FOURCC_NV12, info,
                                        mfx_frame);
           } else {
               MFX_DEBUG_TRACE_PRINTF("unsupported format");
               res = C2_BAD_VALUE;
               break;
           }
        }

        frame_converter_ = frame_converter;
        mfx_frame_surface_ = mfx_frame;
        c2_buffer_ = std::move(buf_pack.buffers.front());

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}
