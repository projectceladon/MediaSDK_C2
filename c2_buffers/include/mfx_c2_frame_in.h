/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2021 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_frame_converter.h"
#include "mfx_defs.h"
#include "mfx_c2_vpp_wrapp.h"

#include <C2Buffer.h>
#include <C2Work.h>

class MfxC2FrameIn
{
public:
    MfxC2FrameIn() = default;
    MfxC2FrameIn(MfxC2FrameIn&& other) = default;
    ~MfxC2FrameIn();

    static c2_status_t Create(std::shared_ptr<MfxFrameConverter> frame_converter,  std::unique_ptr<const C2GraphicView> c_graph_view,
        C2FrameData& buf_pack, mfxFrameSurface1 *mfx_frame, MfxC2FrameIn* wrapper);

    static c2_status_t Create(std::shared_ptr<MfxFrameConverter> frame_converter,
        C2FrameData& buf_pack, const mfxFrameInfo& info, c2_nsecs_t timeout, MfxC2FrameIn* wrapper);

    mfxFrameSurface1* GetMfxFrameSurface() const
    {
        return mfx_frame_;
    }
private:
    std::shared_ptr<C2Buffer> c2_buffer_;
    std::unique_ptr<const C2GraphicView> c2_graphic_view_;
    mfxFrameSurface1 *mfx_frame_;
    std::shared_ptr<MfxFrameConverter> frame_converter_;
};
