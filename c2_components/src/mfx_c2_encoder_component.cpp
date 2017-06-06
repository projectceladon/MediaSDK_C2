/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_encoder_component.h"

#include "mfx_debug.h"
#include "mfx_c2_components_registry.h"

using namespace android;

MfxC2EncoderComponent::MfxC2EncoderComponent(const android::C2String name, int flags, EncoderType encoder_type) :
    MfxC2Component(name, flags), encoder_type_(encoder_type)
{
    MFX_DEBUG_TRACE_FUNC;
}

void MfxC2EncoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("C2.h264ve", &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H264>);
}

android::status_t MfxC2EncoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    return C2_OK;
}
