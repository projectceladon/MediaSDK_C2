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

#include "mfx_defaults.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#include <memory.h>
#include <cassert>

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_defaults"

void mfx_set_defaults_mfxFrameInfo(mfxFrameInfo* info)
{
    MFX_DEBUG_TRACE_FUNC;

    if (!info) return;
    memset(info, 0, sizeof(mfxFrameInfo));

    info->BitDepthLuma = 8;
    info->BitDepthChroma = 8;
    info->FourCC = MFX_FOURCC_NV12;
    info->ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    info->Width = 320;
    info->Height = 240;
    info->CropX = 0;
    info->CropY = 0;
    info->CropW = 320;
    info->CropH = 240;
    info->FrameRateExtN = 30;
    info->FrameRateExtD = 1;
    info->PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
}

void mfx_set_defaults_mfxVideoParam_dec(mfxVideoParam* params)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxU32 CodecId = 0;

    if (!params) return;
    CodecId = params->mfx.CodecId;

    mfx_set_defaults_mfxFrameInfo(&params->mfx.FrameInfo);

    memset(params, 0, sizeof(mfxVideoParam));
    params->AsyncDepth = 0;
    params->mfx.CodecId = CodecId;
    params->mfx.NumThread = 0;

    MFX_DEBUG_TRACE__mfxVideoParam_dec((*params))
}

void mfx_set_defaults_mfxVideoParam_vpp(mfxVideoParam* params)
{
    MFX_DEBUG_TRACE_FUNC;

    if (!params) return;
    memset(params, 0, sizeof(mfxVideoParam));
    /** @todo For vpp it is needed to set extended parameters also. */
}

mfxStatus mfx_set_RateControlMethod(mfxU16 rate_control_method, mfxVideoParam* params)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;

    params->mfx.RateControlMethod = rate_control_method;

    switch(rate_control_method) {
        case MFX_RATECONTROL_CBR:
        case MFX_RATECONTROL_VBR:

            switch (params->mfx.CodecId)
            {
                case MFX_CODEC_AVC:
                    params->mfx.TargetKbps = 2222;
                    break;
                case MFX_CODEC_MPEG2:
                    params->mfx.TargetKbps = 5000;
                    break;
                case MFX_CODEC_VP8:
                    params->mfx.TargetKbps = 2000;
                    break;
                case MFX_CODEC_HEVC:
                    params->mfx.TargetKbps = 3000;
                    break;
                default:
                    MFX_DEBUG_TRACE_MSG("Unsupported Codec ID");
                    res = MFX_ERR_INVALID_VIDEO_PARAM;
            }
            break;

        case MFX_RATECONTROL_CQP: {
            mfxU16 cqp_value = 30;
            params->mfx.QPI = cqp_value;
            params->mfx.QPP = cqp_value;
            params->mfx.QPB = cqp_value;
            break;
        }
        default:
            MFX_DEBUG_TRACE_MSG("Unsupported Rate Control Method");
            res = MFX_ERR_INVALID_VIDEO_PARAM;
    }

    return res;
}

mfxStatus mfx_set_defaults_mfxVideoParam_enc(mfxVideoParam* params)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;

    do {
        mfxU32 CodecId = 0;

        if (!params) {
            res = MFX_ERR_NULL_PTR;
            break;
        }

        CodecId = params->mfx.CodecId;

        memset(params, 0, sizeof(mfxVideoParam));
        params->mfx.CodecId = CodecId;
        params->mfx.NumThread = 0;

        mfx_set_defaults_mfxFrameInfo(&params->mfx.FrameInfo);

        switch (params->mfx.CodecId)
        {
        case MFX_CODEC_AVC:
            // Setting mimimum number of parameters:
            //  - TargetUsage: best speed: to mimimize number of used features
            //  - RateControlMethod: constant bitrate
            //  - PicStruct: progressive
            //  - GopRefDist: 1: to exclude B-frames which can be unsupported on some devices
            //  - GopPicSize: 15: some
            //  - NumSlice: 1
            params->mfx.CodecProfile = MFX_PROFILE_AVC_CONSTRAINED_BASELINE;
            params->mfx.CodecLevel = MFX_LEVEL_AVC_51;
            params->mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
            params->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            res = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, params);
            params->mfx.GopRefDist = 1;
            params->mfx.GopPicSize = 15;
            params->mfx.NumSlice = 1;
            break;
        case MFX_CODEC_MPEG2:
            // Setting mimimum number of parameters:
            //  - TargetUsage: best speed: to mimimize number of used features
            //  - RateControlMethod: constant bitrate
            //  - PicStruct: progressive
            //  - TargetKbps: 5000: some
            //  - GopRefDist: 1: to exclude B-frames which can be unsupported on some devices
            //  - GopPicSize: 15: some
            params->mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
            params->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            res = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, params);
            params->mfx.GopRefDist = 1;
            params->mfx.GopPicSize = 15;
            break;
        case MFX_CODEC_VP8:
            params->mfx.CodecProfile = MFX_PROFILE_UNKNOWN;
            params->mfx.CodecLevel = MFX_LEVEL_UNKNOWN;
            params->mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
            params->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            res = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, params);
            params->mfx.GopPicSize = 0;
            break;
        case MFX_CODEC_HEVC:
            params->mfx.CodecProfile = MFX_PROFILE_HEVC_MAIN;
            params->mfx.CodecLevel = MFX_LEVEL_HEVC_6;
            params->mfx.TargetUsage = MFX_TARGETUSAGE_BEST_SPEED;
            params->mfx.FrameInfo.PicStruct = MFX_PICSTRUCT_PROGRESSIVE;
            res = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, params);
            params->mfx.GopPicSize = 16;
            params->mfx.GopRefDist = 1;
            params->mfx.NumSlice = 1;
            params->mfx.NumRefFrame = 1;
            break;
        default:
            break;
        };

        MFX_DEBUG_TRACE__mfxVideoParam_enc((*params))

    } while(false);

    return res;
}
