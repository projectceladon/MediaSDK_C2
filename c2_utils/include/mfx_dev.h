/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"
#include <memory>

class MfxDev
{
public:
    virtual ~MfxDev() = default;

public:
    virtual mfxStatus Init() = 0;

    virtual mfxStatus Close() = 0;

    virtual mfxStatus InitMfxSession(MFXVideoSession* session) = 0;

    static mfxStatus Create(std::unique_ptr<MfxDev>* device);
};
