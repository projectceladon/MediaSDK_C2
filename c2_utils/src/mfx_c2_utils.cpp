/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_utils.h"
#include "mfx_debug.h"

using namespace android;

status_t MfxStatusToC2(mfxStatus mfx_status)
{
    switch(mfx_status) {
        case MFX_ERR_NONE:
            return C2_OK;

        case MFX_ERR_NULL_PTR:
        case MFX_ERR_INVALID_HANDLE:
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        case MFX_ERR_INVALID_VIDEO_PARAM:
        case MFX_ERR_INCOMPATIBLE_AUDIO_PARAM:
        case MFX_ERR_INVALID_AUDIO_PARAM:
            return C2_BAD_VALUE;

        case MFX_ERR_UNSUPPORTED:
            return C2_UNSUPPORTED;

        case MFX_ERR_NOT_FOUND:
            return C2_NOT_FOUND;

        case MFX_ERR_MORE_BITSTREAM:
        case MFX_ERR_MORE_DATA:
        case MFX_ERR_MORE_SURFACE:
        case MFX_ERR_NOT_INITIALIZED:
            return C2_BAD_STATE;

        case MFX_ERR_MEMORY_ALLOC:
        case MFX_ERR_NOT_ENOUGH_BUFFER:
        case MFX_ERR_LOCK_MEMORY:
            return C2_NO_MEMORY;

        case MFX_ERR_GPU_HANG:
            return C2_TIMED_OUT;

        case MFX_ERR_UNKNOWN:
        case MFX_ERR_UNDEFINED_BEHAVIOR:
        case MFX_ERR_DEVICE_FAILED:
        case MFX_ERR_ABORTED:
        case MFX_ERR_DEVICE_LOST:
        default:
            return C2_CORRUPTED;
    }
}

status_t GetC2ConstGraphicBlock(
    const C2BufferPack& buf_pack, std::unique_ptr<C2ConstGraphicBlock>* c_graph_block)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_BAD_VALUE;

    do {
        if(nullptr == c_graph_block) break;

        if(buf_pack.buffers.size() != 1) break;

        std::shared_ptr<C2Buffer> in_buffer = buf_pack.buffers.front();
        if(nullptr == in_buffer) break;

        const C2BufferData& in_buf_data = in_buffer->data();
        if(in_buf_data.type() != C2BufferData::GRAPHIC) break;

        (*c_graph_block) = std::make_unique<C2ConstGraphicBlock>(in_buf_data.graphicBlocks().front());

        res = C2_OK;

    } while(false);

    return res;
}

status_t GetC2ConstLinearBlock(
    const C2BufferPack& buf_pack, std::unique_ptr<C2ConstLinearBlock>* c_lin_block)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_BAD_VALUE;

    do {
        if(nullptr == c_lin_block) break;

        if(buf_pack.buffers.size() != 1) break;

        std::shared_ptr<C2Buffer> in_buffer = buf_pack.buffers.front();
        if(nullptr == in_buffer) break;

        const C2BufferData& in_buf_data = in_buffer->data();
        if(in_buf_data.type() != C2BufferData::LINEAR) break;

        (*c_lin_block) = std::make_unique<C2ConstLinearBlock>(in_buf_data.linearBlocks().front());

        res = C2_OK;

    } while(false);

    return res;
}

status_t MapConstGraphicBlock(
    const C2ConstGraphicBlock& c_graph_block, nsecs_t timeout, const uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        res = c_graph_block.fence().wait(timeout);
        if(C2_OK != res) break;

        C2Acquirable<const C2GraphicView> acquirable = c_graph_block.map();

        res = acquirable.wait(timeout);
        if(C2_OK != res) break;

        C2GraphicView graph_view = acquirable.get();
        *data = graph_view.data();

    } while(false);

    return res;
}

status_t MapGraphicBlock(
    C2GraphicBlock& graph_block, nsecs_t timeout, uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        C2Acquirable<C2GraphicView> acq_graph_view = graph_block.map();

        res = acq_graph_view.wait(timeout);
        if(C2_OK != res) break;

        C2GraphicView gr_view = acq_graph_view.get();
        *data = gr_view.data();

    } while(false);

    return res;
}

status_t MapConstLinearBlock(
    const C2ConstLinearBlock& c_lin_block, nsecs_t timeout, const uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        res = c_lin_block.fence().wait(timeout);
        if(C2_OK != res) break;

        C2Acquirable<C2ReadView> acq_read_view = c_lin_block.map();

        res = acq_read_view.wait(timeout);
        if(C2_OK != res) break;

        C2ReadView read_view = acq_read_view.get();
        *data = read_view.data();

    } while(false);

    return res;
}

status_t MapLinearBlock(
    C2LinearBlock& lin_block, nsecs_t timeout, uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        C2Acquirable<C2WriteView> acq_write_view = lin_block.map();

        res = acq_write_view.wait(timeout);
        if(C2_OK != res) break;

        C2WriteView write_view = acq_write_view.get();
        *data = write_view.data();

    } while(false);

    return res;
}
