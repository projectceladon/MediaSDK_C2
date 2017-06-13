/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#ifdef LIBVA_SUPPORT

#include "mfx_dev.h"

#define MFX_VA_ANDROID_DISPLAY_ID 0x18c34078

class MfxDevVa : public MfxDev
{
public:
    MfxDevVa();
    virtual ~MfxDevVa();

public:
    virtual mfxStatus Init() override;
    virtual mfxStatus Close() override;
    virtual mfxStatus InitMfxSession(MFXVideoSession* session) override;

protected:
    typedef unsigned int MfxVaAndroidDisplayId;

protected:
    bool initialized_;
    MfxVaAndroidDisplayId display_id_ = MFX_VA_ANDROID_DISPLAY_ID;
    VADisplay va_display_;

private:
    MFX_CLASS_NO_COPY(MfxDevVa)
};

#endif // #ifdef LIBVA_SUPPORT
