/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_bitstream_in.h"
#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_msdk_debug.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_bitstream_in"

MfxC2BitstreamIn::MfxC2BitstreamIn(MfxC2FrameConstructorType fc_type)
{
    MFX_DEBUG_TRACE_FUNC;

    frame_constructor_ = MfxC2FrameConstructorFactory::CreateFrameConstructor(fc_type);
}

MfxC2BitstreamIn::~MfxC2BitstreamIn()
{
    MFX_DEBUG_TRACE_FUNC;
}

c2_status_t MfxC2BitstreamIn::Reset()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        mfxStatus mfx_res = frame_constructor_->Reset();
        res = MfxStatusToC2(mfx_res);
        if(C2_OK != res) break;
    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2BitstreamIn::AppendFrame(const C2FrameData& buf_pack, c2_nsecs_t timeout,
    std::unique_ptr<FrameView>* frame_view)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    const mfxU8* data = nullptr;
    mfxU32 filled_len = 0;

    do {
        if (!frame_view) {
            res = C2_BAD_VALUE;
            break;
        }

        if (buf_pack.buffers.size() == 0) {
            frame_constructor_->SetEosMode(buf_pack.flags & C2FrameData::FLAG_END_OF_STREAM);
            break;
        }

        std::unique_ptr<C2ConstLinearBlock> c_linear_block;
        res = GetC2ConstLinearBlock(buf_pack, &c_linear_block);
        if(C2_OK != res) break;

        std::unique_ptr<C2ReadView> read_view;
        res = MapConstLinearBlock(*c_linear_block, timeout, &read_view);
        if(C2_OK != res) break;

        MFX_DEBUG_TRACE_I64(buf_pack.ordinal.timestamp.peeku());

        data = read_view->data() + c_linear_block->offset();
        filled_len = c_linear_block->size();

        MFX_DEBUG_TRACE_STREAM("data: " << FormatHex(data, filled_len));

        frame_constructor_->SetEosMode(buf_pack.flags & C2FrameData::FLAG_END_OF_STREAM);

        mfxStatus mfx_res = frame_constructor_->Load(data,
                                                     filled_len,
                                                     buf_pack.ordinal.timestamp.peeku(), // pass pts
                                                     buf_pack.flags & C2FrameData::FLAG_CODEC_CONFIG,
                                                     true);
        res = MfxStatusToC2(mfx_res);
        if(C2_OK != res) break;

        *frame_view = std::make_unique<FrameView>(frame_constructor_, std::move(read_view));

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2BitstreamIn::FrameView::Release()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    if (read_view_) {
        res = MfxStatusToC2(frame_constructor_->Unload());
        read_view_.reset();
    }
    return res;
}
