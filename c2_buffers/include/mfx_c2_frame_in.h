/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Buffer.h>
#include <C2Work.h>

#include "mfx_defs.h"

class MfxFrameWrapper
{
public:
    MfxFrameWrapper() = default;

    static android::status_t Create(
        android::C2BufferPack& buf_pack, nsecs_t timeout, MfxFrameWrapper* wrapper);

    mfxFrameSurface1* GetMfxFrameSurface() const
    {
        return mfx_frame_.get();
    }
private:
    std::shared_ptr<android::C2Buffer> c2_buffer_;
    std::unique_ptr<mfxFrameSurface1> mfx_frame_;
};
