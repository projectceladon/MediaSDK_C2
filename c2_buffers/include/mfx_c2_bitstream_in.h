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
#include "mfx_frame_constructor.h"

class MfxC2BitstreamIn
{
public:
    MfxC2BitstreamIn(MfxC2FrameConstructorType fc_type);
    virtual ~MfxC2BitstreamIn();

    virtual std::shared_ptr<IMfxC2FrameConstructor> GetFrameConstructor() { return frame_constructor_; }

    virtual status_t LoadC2BufferPack(android::C2BufferPack& buf_pack, nsecs_t timeout);

protected: // variables
    std::shared_ptr<IMfxC2FrameConstructor> frame_constructor_;

private:
    MFX_CLASS_NO_COPY(MfxC2BitstreamIn)
};
