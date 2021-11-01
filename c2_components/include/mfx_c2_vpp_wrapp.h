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


#ifndef __MFX_C2_VPP_WRAPP_H__
#define __MFX_C2_VPP_WRAPP_H__

#include <stdio.h>
#include <utility>
#include <vector>

#include "mfx_c2_defs.h"
#include "mfx_dev.h"

#define VPP_MAX_SRF_NUM 10

enum MfxC2Conversion
{
    CONVERT_NONE,
    NV12_TO_ARGB,
    ARGB_TO_NV12
};

struct MfxC2VppWrappParam
{
#ifdef USE_ONEVPL
    mfxSession         session;
#else
    MFXVideoSession   *session;
#endif
    mfxFrameInfo      *frame_info;
    std::shared_ptr<MfxFrameAllocator> allocator;

    MfxC2Conversion   conversion;
};

class MfxC2VppWrapp
{
public:
    MfxC2VppWrapp(void);
    ~MfxC2VppWrapp(void);

    mfxStatus Init(MfxC2VppWrappParam *param);
    mfxStatus Close(void);
    mfxStatus ProcessFrameVpp(mfxFrameSurface1 *in_srf, mfxFrameSurface1 **out_srf);

protected:
    mfxStatus FillVppParams(mfxFrameInfo *frame_info, MfxC2Conversion conversion);
    mfxStatus AllocateOneSurface(void);

    MFXVideoVPP *m_pVpp;
#ifdef USE_ONEVPL
    mfxSession m_mfxSession;
#else
    MFXVideoSession *m_pSession;
#endif
    mfxVideoParam m_vppParam;
    std::shared_ptr<MfxFrameAllocator> m_allocator;

    mfxFrameAllocResponse m_responses[VPP_MAX_SRF_NUM];
    mfxFrameSurface1 m_vppSrf[VPP_MAX_SRF_NUM];
    mfxU32 m_uVppSurfaceCount;

private:
    MFX_CLASS_NO_COPY(MfxC2VppWrapp)
};

#endif // #ifndef __MFX_C2_VPP_WRAPP_H__
