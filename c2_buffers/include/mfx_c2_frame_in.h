/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_frame_converter.h"
#include "mfx_defs.h"

#include <C2Buffer.h>
#include <C2Work.h>

class MfxC2FrameIn
{
public:
    MfxC2FrameIn() = default;
    MfxC2FrameIn(MfxC2FrameIn&& other) = default;
    ~MfxC2FrameIn();

    static c2_status_t Create(MfxFrameConverter* frame_converter,
        C2FrameData& buf_pack, nsecs_t timeout, MfxC2FrameIn* wrapper);

    mfxFrameSurface1* GetMfxFrameSurface() const
    {
        return mfx_frame_.get();
    }
private:
    std::shared_ptr<C2Buffer> c2_buffer_;
    std::unique_ptr<const C2GraphicView> c2_graphic_view_;
    std::unique_ptr<mfxFrameSurface1> mfx_frame_;
    MfxFrameConverter* frame_converter_ {};
};
