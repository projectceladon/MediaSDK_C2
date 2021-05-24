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


#include "mfx_c2_vpp_wrapp.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_utils.h"
#include "mfx_msdk_debug.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_vpp_wrapp"

MfxC2VppWrapp::MfxC2VppWrapp(void):
    vpp_(NULL),
    session_(NULL),
    num_vpp_surfaces_(0)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_ZERO_MEMORY(vpp_param_);
    MFX_ZERO_MEMORY(allocator_);
    MFX_ZERO_MEMORY(responses_);
}

MfxC2VppWrapp::~MfxC2VppWrapp(void)
{
    MFX_DEBUG_TRACE_FUNC;
    Close();
}

mfxStatus MfxC2VppWrapp::Init(MfxC2VppWrappParam *param)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;

    if (!param || !param->session || !param->allocator || !param->frame_info)
        sts = MFX_ERR_NULL_PTR;

    if (MFX_ERR_NONE == sts)
    {
        allocator_ = param->allocator;
        session_ = param->session;

        MFX_NEW(vpp_, MFXVideoVPP(*session_));
        if(!vpp_) sts = MFX_ERR_UNKNOWN;

        if (MFX_ERR_NONE == sts) sts = FillVppParams(param->frame_info, param->conversion);
        MFX_DEBUG_TRACE__mfxFrameInfo(vpp_param_.vpp.In);
        MFX_DEBUG_TRACE__mfxFrameInfo(vpp_param_.vpp.Out);

        if (MFX_ERR_NONE == sts) sts = vpp_->Init(&vpp_param_);
    }

    if (MFX_ERR_NONE == sts) sts = AllocateOneSurface();

    if (MFX_ERR_NONE != sts) Close();

    MFX_DEBUG_TRACE_I32(sts);
    return sts;
}

mfxStatus MfxC2VppWrapp::Close(void)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;

    if (vpp_)
    {
        sts = vpp_->Close();
        MFX_DELETE(vpp_);
        vpp_ = NULL;
    }

    MFX_DEBUG_TRACE_I32(num_vpp_surfaces_);
    if (num_vpp_surfaces_)
    {
        for (mfxU32 i = 0; i < num_vpp_surfaces_; i++)
        {
            allocator_->FreeFrames(&responses_[i]);
        }
    }

    MFX_ZERO_MEMORY(responses_);
    MFX_ZERO_MEMORY(vpp_srf_);
    num_vpp_surfaces_ = 0;
    session_ = NULL;
    allocator_.reset();

    MFX_DEBUG_TRACE_I32(sts);
    return sts;
}

mfxStatus MfxC2VppWrapp::FillVppParams(mfxFrameInfo *frame_info, MfxC2Conversion conversion)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;

    if (!frame_info) sts = MFX_ERR_NULL_PTR;

    if (MFX_ERR_NONE == sts)
    {
        MFX_ZERO_MEMORY(vpp_param_);
        vpp_param_.AsyncDepth = 1;
        vpp_param_.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        vpp_param_.vpp.In = *frame_info;
        vpp_param_.vpp.Out = *frame_info;

        switch (conversion)
        {
            case ARGB_TO_NV12:
                if (MFX_FOURCC_RGB4 != vpp_param_.vpp.In.FourCC)
                    sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

                vpp_param_.vpp.Out.FourCC = MFX_FOURCC_NV12;
                break;

            case NV12_TO_ARGB:
                if (MFX_FOURCC_NV12 != vpp_param_.vpp.In.FourCC)
                    sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

                vpp_param_.vpp.Out.FourCC = MFX_FOURCC_RGB4;
                break;

            case CONVERT_NONE:
                break;
        }

        if (MFX_ERR_NONE != sts)
            MFX_ZERO_MEMORY(vpp_param_);
    }

    MFX_DEBUG_TRACE_I32(sts);
    return sts;
}

mfxStatus MfxC2VppWrapp::AllocateOneSurface(void)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;

    if (num_vpp_surfaces_ >= VPP_MAX_SRF_NUM) sts = MFX_ERR_UNKNOWN;

    if (MFX_ERR_NONE == sts)
    {
        mfxFrameAllocRequest request;
        MFX_ZERO_MEMORY(request);
        request.Info = vpp_param_.vpp.Out;
        request.NumFrameMin = 1;
        request.NumFrameSuggested = 1;
        request.Type = MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT;

        sts = allocator_->AllocFrames(&request, &responses_[num_vpp_surfaces_]);
    }

    if (MFX_ERR_NONE == sts)
    {
        MFX_ZERO_MEMORY(vpp_srf_[num_vpp_surfaces_]);
        vpp_srf_[num_vpp_surfaces_].Info = vpp_param_.vpp.Out;
        vpp_srf_[num_vpp_surfaces_].Data.MemId = responses_[num_vpp_surfaces_].mids[0];
        num_vpp_surfaces_++;
    }

    MFX_DEBUG_TRACE_I32(sts);
    return sts;
}

mfxStatus MfxC2VppWrapp::ProcessFrameVpp(mfxFrameSurface1 *in_srf, mfxFrameSurface1 **out_srf)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameSurface1* outSurface = NULL;
    mfxSyncPoint syncp;

    if (!in_srf || !out_srf) return MFX_ERR_UNKNOWN;

    for (mfxU32 i = 0; i < num_vpp_surfaces_; i++)
    {
        if (false == vpp_srf_[i].Data.Locked)
        {
            outSurface = &vpp_srf_[i];
            break;
        }
    }

    if (num_vpp_surfaces_ < VPP_MAX_SRF_NUM && NULL == outSurface)
    {
        sts = AllocateOneSurface();
        if (MFX_ERR_NONE == sts) outSurface = &vpp_srf_[num_vpp_surfaces_-1]; // just created outSurface
    }

    if (outSurface)
    {
        sts = vpp_->RunFrameVPPAsync(in_srf, outSurface, NULL, &syncp);
        if (MFX_ERR_NONE == sts) sts = session_->SyncOperation(syncp, MFX_TIMEOUT_INFINITE);
    }
    else sts = MFX_ERR_MORE_SURFACE;

    if (MFX_ERR_NONE == sts)
    {
        *out_srf = outSurface;
    }

    MFX_DEBUG_TRACE_I32(sts);
    return sts;
}
