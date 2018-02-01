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

class MfxC2BitstreamOut
{
public:
    MfxC2BitstreamOut() = default;

    static android::c2_status_t Create(
        std::shared_ptr<android::C2LinearBlock> block, nsecs_t timeout,
        MfxC2BitstreamOut* wrapper);

    std::shared_ptr<android::C2LinearBlock> GetC2LinearBlock() const
    {
        return c2_linear_block_;
    }

    mfxBitstream* GetMfxBitstream() const
    {
        return mfx_bitstream_.get();
    }
private:
    std::shared_ptr<android::C2LinearBlock> c2_linear_block_;
    std::unique_ptr<mfxBitstream> mfx_bitstream_;
};
