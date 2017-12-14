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

    static android::status_t Create(MfxFrameConverter* frame_converter,
                                    std::shared_ptr<android::C2GraphicBlock> block,
                                    nsecs_t timeout,
                                    std::unique_ptr<MfxC2FrameOut>& wrapper);

    std::unique_ptr<android::C2Work> GetC2Work();
    std::shared_ptr<android::C2GraphicBlock> GetC2GraphicBlock() const;
    mfxFrameSurface1* GetMfxFrameSurface() const;
    void PutC2Work(std::unique_ptr<android::C2Work>&& work);

private:
    std::unique_ptr<android::C2Work> work_;
    std::shared_ptr<android::C2GraphicBlock> c2_graphic_block_;
    std::unique_ptr<mfxFrameSurface1> mfx_surface_;
    MfxFrameConverter* frame_converter_ {};
};

class MfxC2FrameOutPool
{
public:
    MfxC2FrameOutPool() = default;

    void AddFrame(std::unique_ptr<MfxC2FrameOut>&& frame);
    std::unique_ptr<MfxC2FrameOut> AcquireFrameBySurface(mfxFrameSurface1* surface);
    std::unique_ptr<MfxC2FrameOut> AcquireUnlockedFrame();
    size_t Size() const { return frame_pool_.size(); }
private:
    std::list<std::unique_ptr<MfxC2FrameOut>> frame_pool_;
};
