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

#include "mfx_frame_converter.h"
#include "mfx_defs.h"

class MfxC2FrameOut
{
public:
    MfxC2FrameOut() = default;

    MfxC2FrameOut(std::shared_ptr<android::C2GraphicBlock>&& c2_block,
        std::shared_ptr<mfxFrameSurface1> mfx_frame)
        : c2_graphic_block_(std::move(c2_block))
        , mfx_surface_(mfx_frame)
    {}

    static android::status_t Create(MfxFrameConverter* frame_converter,
                                    std::shared_ptr<android::C2GraphicBlock> block,
                                    nsecs_t timeout,
                                    MfxC2FrameOut* wrapper);

    std::shared_ptr<android::C2GraphicBlock> GetC2GraphicBlock() const;
    std::shared_ptr<mfxFrameSurface1> GetMfxFrameSurface() const;

private:
    std::shared_ptr<android::C2GraphicBlock> c2_graphic_block_;
    std::shared_ptr<mfxFrameSurface1> mfx_surface_;
};
