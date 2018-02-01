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

c2_status_t MfxC2FrameIn::Create(MfxFrameConverter* frame_converter,
    C2BufferPack& buf_pack, nsecs_t timeout, MfxC2FrameIn* wrapper)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

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
            std::unique_ptr<const C2GraphicView> c_graph_view;
            res = MapConstGraphicBlock(*c_graph_block, timeout, &c_graph_view);
            if(C2_OK != res) break;

            InitMfxNV12FrameSW(buf_pack.ordinal.timestamp, buf_pack.ordinal.frame_index,
                c_graph_view->data(), c_graph_block->width(), c_graph_block->height(),
                unique_mfx_frame.get());
        }

        wrapper->frame_converter_ = frame_converter;
        wrapper->mfx_frame_ = std::move(unique_mfx_frame);
        wrapper->c2_buffer_ = std::move(buf_pack.buffers.front());

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}
