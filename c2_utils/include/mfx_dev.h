/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"
#include "mfx_allocator.h"
#include "mfx_frame_converter.h"
#include "mfx_frame_pool_allocator.h"

#include <memory>

class MfxDev
{
public:
    enum class Usage {
        Encoder,
        Decoder
    };
public:
    virtual ~MfxDev() = default;

public:
    virtual mfxStatus Init() = 0;

    virtual mfxStatus Close() = 0;

    virtual MfxFrameAllocator* GetFrameAllocator() = 0;

    virtual MfxFrameConverter* GetFrameConverter() = 0;

    virtual MfxFramePoolAllocator* GetFramePoolAllocator() = 0;

    virtual mfxStatus InitMfxSession(MFXVideoSession* session) = 0;

    static mfxStatus Create(Usage usage, std::unique_ptr<MfxDev>* device);
};
