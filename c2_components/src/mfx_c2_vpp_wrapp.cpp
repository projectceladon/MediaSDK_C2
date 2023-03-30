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
    m_pVpp(NULL),
#ifdef USE_ONEVPL
    m_mfxSession(NULL),
#else
    m_pSession(NULL),
#endif
    m_uVppSurfaceCount(0)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_ZERO_MEMORY(m_vppParam);
    MFX_ZERO_MEMORY(m_allocator);
    MFX_ZERO_MEMORY(m_responses);
    MFX_ZERO_MEMORY(m_vppSrf);
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
        m_allocator = param->allocator;
#ifdef USE_ONEVPL
        m_mfxSession = param->session;
        MFX_NEW(m_pVpp, MFXVideoVPP(m_mfxSession));
#else
        m_pSession = param->session;
        MFX_NEW(m_pVpp, MFXVideoVPP(*m_pSession));
#endif

        if(!m_pVpp) sts = MFX_ERR_UNKNOWN;

        if (MFX_ERR_NONE == sts) sts = FillVppParams(param->frame_info, param->conversion);
        MFX_DEBUG_TRACE__mfxFrameInfo(m_vppParam.vpp.In);
        MFX_DEBUG_TRACE__mfxFrameInfo(m_vppParam.vpp.Out);

        if (MFX_ERR_NONE == sts) sts = m_pVpp->Init(&m_vppParam);
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

    if (m_pVpp)
    {
        sts = m_pVpp->Close();
        MFX_DELETE(m_pVpp);
        m_pVpp = NULL;
    }

    MFX_DEBUG_TRACE_I32(m_uVppSurfaceCount);
    if (m_uVppSurfaceCount)
    {
        for (mfxU32 i = 0; i < m_uVppSurfaceCount; i++)
        {
            m_allocator->FreeFrames(&m_responses[i]);
        }
    }

    MFX_ZERO_MEMORY(m_responses);
    MFX_ZERO_MEMORY(m_vppSrf);
    m_uVppSurfaceCount = 0;
#ifdef USE_ONEVPL
    m_mfxSession = NULL;
#else
    m_pSession = NULL;
#endif
    m_allocator.reset();

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
        MFX_ZERO_MEMORY(m_vppParam);
        m_vppParam.AsyncDepth = 1;
        m_vppParam.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
        m_vppParam.vpp.In = *frame_info;
        m_vppParam.vpp.Out = *frame_info;

        switch (conversion)
        {
            case ARGB_TO_NV12:
                if (MFX_FOURCC_RGB4 != m_vppParam.vpp.In.FourCC)
                    sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

                m_vppParam.vpp.Out.FourCC = MFX_FOURCC_NV12;
                break;

            case NV12_TO_ARGB:
                if (MFX_FOURCC_NV12 != m_vppParam.vpp.In.FourCC)
                    sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;

                m_vppParam.vpp.Out.FourCC = MFX_FOURCC_RGB4;
                break;

            case CONVERT_NONE:
                break;
        }

        if (MFX_ERR_NONE != sts)
            MFX_ZERO_MEMORY(m_vppParam);
    }

    MFX_DEBUG_TRACE_I32(sts);
    return sts;
}

mfxStatus MfxC2VppWrapp::AllocateOneSurface(void)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;

    if (m_uVppSurfaceCount >= VPP_MAX_SRF_NUM) sts = MFX_ERR_UNKNOWN;

    if (MFX_ERR_NONE == sts)
    {
        mfxFrameAllocRequest request;
        MFX_ZERO_MEMORY(request);
        request.Info = m_vppParam.vpp.Out;
        request.NumFrameMin = 1;
        request.NumFrameSuggested = 1;
        request.Type = MFX_MEMTYPE_VIDEO_MEMORY_PROCESSOR_TARGET | MFX_MEMTYPE_FROM_VPPOUT;

        sts = m_allocator->AllocFrames(&request, &m_responses[m_uVppSurfaceCount]);
    }

    if (MFX_ERR_NONE == sts)
    {
        MFX_ZERO_MEMORY(m_vppSrf[m_uVppSurfaceCount]);
        m_vppSrf[m_uVppSurfaceCount].Info = m_vppParam.vpp.Out;
        m_vppSrf[m_uVppSurfaceCount].Data.MemId = m_responses[m_uVppSurfaceCount].mids[0];
        m_uVppSurfaceCount++;
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

    for (mfxU32 i = 0; i < m_uVppSurfaceCount; i++)
    {
        if (false == m_vppSrf[i].Data.Locked)
        {
            outSurface = &m_vppSrf[i];
            break;
        }
    }

    if (m_uVppSurfaceCount < VPP_MAX_SRF_NUM && NULL == outSurface)
    {
        sts = AllocateOneSurface();
        if (MFX_ERR_NONE == sts) outSurface = &m_vppSrf[m_uVppSurfaceCount-1]; // just created outSurface
    }

    if (outSurface)
    {
        sts = m_pVpp->RunFrameVPPAsync(in_srf, outSurface, NULL, &syncp);
        if (MFX_ERR_NONE == sts)
#ifdef USE_ONEVPL
            sts = MFXVideoCORE_SyncOperation(m_mfxSession, syncp, MFX_TIMEOUT_INFINITE);
#else
            sts = m_pSession->SyncOperation(syncp, MFX_TIMEOUT_INFINITE);
#endif
    }
    else sts = MFX_ERR_MORE_SURFACE;

    if (MFX_ERR_NONE == sts)
    {
        *out_srf = outSurface;
    }

    MFX_DEBUG_TRACE_I32(sts);
    return sts;
}