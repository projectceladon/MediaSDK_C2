/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

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

    MfxC2FrameOut(std::shared_ptr<C2GraphicBlock>&& c2_block,
        std::shared_ptr<mfxFrameSurface1> mfx_frame)
        : c2_graphic_block_(std::move(c2_block))
        , mfx_surface_(mfx_frame)
    {}

    static c2_status_t Create(const std::shared_ptr<MfxFrameConverter>& frame_converter,
                                    std::shared_ptr<C2GraphicBlock> block,
                                    const mfxFrameInfo& info,
                                    c2_nsecs_t timeout,
                                    MfxC2FrameOut* wrapper);

    std::shared_ptr<C2GraphicBlock> GetC2GraphicBlock() const;
    std::shared_ptr<C2GraphicView> GetC2GraphicView() const;
    std::shared_ptr<mfxFrameSurface1> GetMfxFrameSurface() const;

    bool operator==(const MfxC2FrameOut& other) const {
        return
            c2_graphic_block_ == other.c2_graphic_block_ &&
            c2_graphic_view_ == other.c2_graphic_view_ &&
            mfx_surface_ == other.mfx_surface_;
    }
private:
    std::shared_ptr<C2GraphicBlock> c2_graphic_block_;
    std::shared_ptr<C2GraphicView> c2_graphic_view_;
    std::shared_ptr<mfxFrameSurface1> mfx_surface_;
};
