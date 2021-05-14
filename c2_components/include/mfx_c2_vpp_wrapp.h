/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2021 Intel Corporation. All Rights Reserved.

*********************************************************************************/

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
    MFXVideoSession   *session;
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

    MFXVideoVPP *vpp_;
    MFXVideoSession *session_;
    mfxVideoParam vpp_param_;
    std::shared_ptr<MfxFrameAllocator> allocator_;

    mfxFrameAllocResponse responses_[VPP_MAX_SRF_NUM];
    mfxFrameSurface1 vpp_srf_[VPP_MAX_SRF_NUM];
    mfxU32 num_vpp_surfaces_;

private:
    MFX_CLASS_NO_COPY(MfxC2VppWrapp)
};

#endif // #ifndef __MFX_C2_VPP_WRAPP_H__
