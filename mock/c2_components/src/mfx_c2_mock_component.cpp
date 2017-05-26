/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_mock_component.h"

#include "mfx_debug.h"
#include "mfx_c2_components_registry.h"

using namespace android;

MfxC2MockComponent::MfxC2MockComponent(const android::C2String name, int flags) :
    MfxC2Component(name, flags)
{
    MFX_DEBUG_TRACE_FUNC;
}

void MfxC2MockComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;
    registry.RegisterMfxC2Component("C2.MockComponent", &MfxC2Component::Create<MfxC2MockComponent>);
}

android::status_t MfxC2MockComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    return C2_OK;
}
