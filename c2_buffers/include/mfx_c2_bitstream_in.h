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

#include "mfx_defs.h"
#include "mfx_frame_constructor.h"

class MfxC2BitstreamIn
{
public:
    class FrameView
    {
    public:
        FrameView(std::shared_ptr<IMfxC2FrameConstructor> frame_constructor,
            std::unique_ptr<C2ReadView>&& read_view):
                frame_constructor_(frame_constructor), read_view_(std::move(read_view)) {}
        ~FrameView() { Release(); }

        c2_status_t Release();

    private:
        std::shared_ptr<IMfxC2FrameConstructor> frame_constructor_;
        std::unique_ptr<C2ReadView> read_view_;

    private:
        MFX_CLASS_NO_COPY(FrameView)
    };

public:
    MfxC2BitstreamIn(MfxC2FrameConstructorType fc_type);
    virtual ~MfxC2BitstreamIn();

    virtual c2_status_t Reset();

    virtual std::shared_ptr<IMfxC2FrameConstructor> GetFrameConstructor() { return frame_constructor_; }
    // Maps c2 linear block and can leave it in mapped state until
    // frame_view freed or frame_view->Release is called.
    virtual c2_status_t AppendFrame(const C2FrameData& buf_pack, c2_nsecs_t timeout,
        std::unique_ptr<FrameView>* frame_view);

protected: // variables
    std::shared_ptr<IMfxC2FrameConstructor> frame_constructor_;

private:
    MFX_CLASS_NO_COPY(MfxC2BitstreamIn)
};
