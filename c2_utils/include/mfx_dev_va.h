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
#include "mfx_va_allocator.h"
#include "mfx_va_frame_pool_allocator.h"

#define MFX_VA_ANDROID_DISPLAY_ID 0x18c34078

class MfxDevVa : public MfxDev
{
public:
    MfxDevVa(Usage usage);
    virtual ~MfxDevVa();

public:
    VADisplay GetVaDisplay() { return va_display_; }

private:
    virtual mfxStatus Init() override;
    virtual mfxStatus Close() override;
    MfxFrameAllocator* GetFrameAllocator() override;
    MfxFrameConverter* GetFrameConverter() override;
    virtual MfxFramePoolAllocator* GetFramePoolAllocator() override;
    virtual mfxStatus InitMfxSession(MFXVideoSession* session) override;

protected:
    typedef unsigned int MfxVaAndroidDisplayId;

protected:
    Usage usage_ {};
    bool va_initialized_ { false };
    MfxVaAndroidDisplayId display_id_ = MFX_VA_ANDROID_DISPLAY_ID;
    VADisplay va_display_ { nullptr };
    std::unique_ptr<MfxVaFrameAllocator> va_allocator_;
    std::unique_ptr<MfxVaFramePoolAllocator> va_pool_allocator_;

private:
    MFX_CLASS_NO_COPY(MfxDevVa)
};

#endif // #ifdef LIBVA_SUPPORT
