/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_dev.h"

class MfxC2EncoderComponent : public MfxC2Component
{
public:
    enum EncoderType {
        ENCODER_H264,
    };

protected:
    MfxC2EncoderComponent(const android::C2String name, int flags, EncoderType encoder_type);

    MFX_CLASS_NO_COPY(MfxC2EncoderComponent)

public:
    virtual ~MfxC2EncoderComponent();

public:
    static void RegisterClass(MfxC2ComponentsRegistry& registry);

protected:
    android::status_t Init() override;

private:
    EncoderType encoder_type_;
    std::unique_ptr<MfxDev> device_;
    MFXVideoSession session_;
    std::unique_ptr<MFXVideoENCODE> encoder_;
};
