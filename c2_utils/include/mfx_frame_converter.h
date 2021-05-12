/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"

#include <hardware/gralloc.h>

class MfxFrameConverter
{
public:
    virtual mfxStatus ConvertGrallocToVa(buffer_handle_t gralloc_buffer,
        bool decode_target, mfxMemId* mem_id) = 0;

    virtual void FreeGrallocToVaMapping(mfxMemId mem_id) = 0;

    virtual void FreeAllMappings() = 0;

protected: // virtual deletion prohibited
    virtual ~MfxFrameConverter() = default;
};

