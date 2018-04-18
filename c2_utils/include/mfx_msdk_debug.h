/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_debug.h"
#include "mfxdefs.h"

MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(mfxStatus)

#define MFX_DEBUG_TRACE__mfxFrameInfo(_s) \
    MFX_DEBUG_TRACE_U32(_s.FourCC); \
    MFX_DEBUG_TRACE_I32(_s.Width); \
    MFX_DEBUG_TRACE_I32(_s.Height); \
    MFX_DEBUG_TRACE_I32(_s.CropX); \
    MFX_DEBUG_TRACE_I32(_s.CropY); \
    MFX_DEBUG_TRACE_I32(_s.CropW); \
    MFX_DEBUG_TRACE_I32(_s.CropH); \
    MFX_DEBUG_TRACE_I32(_s.FrameRateExtN); \
    MFX_DEBUG_TRACE_I32(_s.FrameRateExtD); \
    MFX_DEBUG_TRACE_I32(_s.AspectRatioW); \
    MFX_DEBUG_TRACE_I32(_s.AspectRatioH); \
    MFX_DEBUG_TRACE_I32(_s.PicStruct); \
    MFX_DEBUG_TRACE_I32(_s.ChromaFormat);

#if MFX_DEBUG == MFX_DEBUG_YES
    #define MFX_DEBUG_TRACE__mfxInfoMFX_enc_RC_method(_s) \
        MFX_DEBUG_TRACE_I32(_s.RateControlMethod); \
        if ((MFX_RATECONTROL_CBR == _s.RateControlMethod) || \
            (MFX_RATECONTROL_VBR == _s.RateControlMethod)) { \
            MFX_DEBUG_TRACE_I32(_s.InitialDelayInKB); \
            MFX_DEBUG_TRACE_I32(_s.BufferSizeInKB); \
            MFX_DEBUG_TRACE_I32(_s.TargetKbps); \
            MFX_DEBUG_TRACE_I32(_s.MaxKbps); \
        } \
        else if (MFX_RATECONTROL_CQP == _s.RateControlMethod) { \
            MFX_DEBUG_TRACE_I32(_s.QPI); \
            MFX_DEBUG_TRACE_I32(_s.BufferSizeInKB); \
            MFX_DEBUG_TRACE_I32(_s.QPP); \
            MFX_DEBUG_TRACE_I32(_s.QPB); \
        } \
        else if (MFX_RATECONTROL_AVBR == _s.RateControlMethod) { \
            MFX_DEBUG_TRACE_I32(_s.Accuracy); \
            MFX_DEBUG_TRACE_I32(_s.BufferSizeInKB); \
            MFX_DEBUG_TRACE_I32(_s.TargetKbps); \
            MFX_DEBUG_TRACE_I32(_s.Convergence); \
        }
#else // #if MFX_DEBUG == MFX_DEBUG_YES
    #define MFX_DEBUG_TRACE__mfxInfoMFX_enc_RC_method(_s)
#endif // #if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__mfxInfoMFX_enc(_s) \
    MFX_DEBUG_TRACE_I32(_s.LowPower); \
    MFX_DEBUG_TRACE_I32(_s.BRCParamMultiplier); \
    MFX_DEBUG_TRACE__mfxFrameInfo(_s.FrameInfo); \
    MFX_DEBUG_TRACE_U32(_s.CodecId); \
    MFX_DEBUG_TRACE_I32(_s.CodecProfile); \
    MFX_DEBUG_TRACE_I32(_s.CodecLevel); \
    MFX_DEBUG_TRACE_I32(_s.NumThread); \
    MFX_DEBUG_TRACE_I32(_s.TargetUsage); \
    MFX_DEBUG_TRACE_I32(_s.GopPicSize); \
    MFX_DEBUG_TRACE_I32(_s.GopRefDist); \
    MFX_DEBUG_TRACE_I32(_s.GopOptFlag); \
    MFX_DEBUG_TRACE_I32(_s.IdrInterval); \
    MFX_DEBUG_TRACE__mfxInfoMFX_enc_RC_method(_s); \
    MFX_DEBUG_TRACE_I32(_s.NumSlice); \
    MFX_DEBUG_TRACE_I32(_s.NumRefFrame); \
    MFX_DEBUG_TRACE_I32(_s.EncodedOrder);

#define MFX_DEBUG_TRACE__mfxExtVPPDeinterlacing(_opt) \
    MFX_DEBUG_TRACE_I32(_opt.Mode); \
    MFX_DEBUG_TRACE_I32(_opt.TelecinePattern); \
    MFX_DEBUG_TRACE_I32(_opt.TelecineLocation);

#define MFX_DEBUG_TRACE__mfxExtVPPDenoise(_opt) \
    MFX_DEBUG_TRACE_I32(_opt.DenoiseFactor);

#if MFX_DEBUG == MFX_DEBUG_YES
    #define MFX_DEBUG_TRACE__mfxExtParams(_num, _params) \
        if(_params) { \
            for (mfxU32 i = 0; i < _num; ++i) { \
                switch (_params[i]->BufferId) { \
                case MFX_EXTBUFF_VPP_DEINTERLACING: \
                    MFX_DEBUG_TRACE__mfxExtVPPDeinterlacing((*((mfxExtVPPDeinterlacing*)_params[i])));\
                    break; \
                case MFX_EXTBUFF_VPP_DENOISE: \
                    MFX_DEBUG_TRACE__mfxExtVPPDenoise((*((mfxExtVPPDenoise*)_params[i])));\
                    break; \
                default: \
                    MFX_DEBUG_TRACE_MSG("unknown ext buffer"); \
                    MFX_DEBUG_TRACE_U32(_params[i]->BufferId); \
                    break; \
                }; \
            } \
        }
#else // #if MFX_DEBUG == MFX_DEBUG_YES
    #define MFX_DEBUG_TRACE__mfxExtParams(_num, _params)
#endif // #if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__mfxVideoParam_enc(_s) \
    MFX_DEBUG_TRACE_I32(_s.AsyncDepth); \
    MFX_DEBUG_TRACE__mfxInfoMFX_enc(_s.mfx); \
    MFX_DEBUG_TRACE_I32(_s.Protected); \
    MFX_DEBUG_TRACE_U32(_s.IOPattern); \
    MFX_DEBUG_TRACE_I32(_s.NumExtParam); \
    MFX_DEBUG_TRACE_P(_s.ExtParam); \
    /*MFX_DEBUG_TRACE__mfxExtParams(_s.NumExtParam, _s.ExtParam)*/

#define MFX_DEBUG_TRACE__mfxVideoParam_vpp(_s) \
    MFX_DEBUG_TRACE_I32(_s.AsyncDepth); \
    MFX_DEBUG_TRACE__mfxFrameInfo(_s.vpp.In) \
    MFX_DEBUG_TRACE__mfxFrameInfo(_s.vpp.Out) \
    MFX_DEBUG_TRACE_I32(_s.Protected); \
    MFX_DEBUG_TRACE_U32(_s.IOPattern); \
    MFX_DEBUG_TRACE_I32(_s.NumExtParam); \
    MFX_DEBUG_TRACE_P(_s.ExtParam); \
    MFX_DEBUG_TRACE__mfxExtParams(_s.NumExtParam, _s.ExtParam)

#define MFX_DEBUG_TRACE__mfxInfoMFX_dec(_s) \
    MFX_DEBUG_TRACE__mfxFrameInfo(_s.FrameInfo); \
    MFX_DEBUG_TRACE_U32(_s.CodecId); \
    MFX_DEBUG_TRACE_I32(_s.CodecProfile); \
    MFX_DEBUG_TRACE_I32(_s.CodecLevel); \
    MFX_DEBUG_TRACE_I32(_s.NumThread); \
    MFX_DEBUG_TRACE_I32(_s.DecodedOrder); \
    MFX_DEBUG_TRACE_I32(_s.ExtendedPicStruct); \

#define MFX_DEBUG_TRACE__mfxVideoParam_dec(_s) \
    MFX_DEBUG_TRACE_I32(_s.AllocId); \
    MFX_DEBUG_TRACE_I32(_s.AsyncDepth); \
    MFX_DEBUG_TRACE__mfxInfoMFX_dec(_s.mfx) \
    MFX_DEBUG_TRACE_I32(_s.Protected); \
    MFX_DEBUG_TRACE_U32(_s.IOPattern); \
    MFX_DEBUG_TRACE_I32(_s.NumExtParam); \
    MFX_DEBUG_TRACE_P(_s.ExtParam); \

#define MFX_DEBUG_TRACE__mfxBitstream(_s) \
    MFX_DEBUG_TRACE_I64(_s.TimeStamp); \
    MFX_DEBUG_TRACE_P(_s.Data); \
    MFX_DEBUG_TRACE_I32(_s.DataOffset); \
    MFX_DEBUG_TRACE_I32(_s.DataLength); \
    MFX_DEBUG_TRACE_I32(_s.MaxLength); \
    MFX_DEBUG_TRACE_I32(_s.PicStruct); \
    MFX_DEBUG_TRACE_I32(_s.FrameType); \
    MFX_DEBUG_TRACE_I32(_s.DataFlag);

#if MFX_DEBUG == MFX_DEBUG_YES
    #define MFX_DEBUG_TRACE__mfxStatus(_e) printf_mfxStatus(MFX_DEBUG_TRACE_VAR, #_e, _e)
#else // #if MFX_DEBUG == MFX_DEBUG_YES
    #define MFX_DEBUG_TRACE__mfxStatus(_e)
#endif // #if MFX_DEBUG == MFX_DEBUG_YES
