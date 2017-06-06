/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_components_registry.h"

#include "mfx_debug.h"
#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_component.h"
#include "mfx_c2_mock_component.h"
#include "mfx_c2_encoder_component.h"

using namespace android;


#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_components_registry"

extern "C" EXPORT status_t MfxCreateC2Component(
    const char* name,
    int flags,
    MfxC2Component** component)
{
    MFX_DEBUG_TRACE_FUNC;

    return MfxC2ComponentsRegistry::getInstance().CreateMfxC2Component(name, flags, component);
}

MfxC2ComponentsRegistry::MfxC2ComponentsRegistry()
{
    // Here should be list of calls like this:
    // MfxC2SomeComponent::RegisterClass();
    // Auto-registration with global variables might not work within static libraries
    MfxC2MockComponent::RegisterClass(*this);
    MfxC2EncoderComponent::RegisterClass(*this);
}

MfxC2ComponentsRegistry& MfxC2ComponentsRegistry::getInstance()
{
    MFX_DEBUG_TRACE_FUNC;

    static MfxC2ComponentsRegistry g_registry;
    return g_registry;
}

status_t MfxC2ComponentsRegistry::CreateMfxC2Component(const char* name, int flags, MfxC2Component** component)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t result = C2_OK;
    *component = nullptr;

    MFX_DEBUG_TRACE_I32(registry_.size());

    auto it = registry_.find(name);
    if(it != registry_.end()) {
        CreateMfxC2ComponentFunc* create_func = it->second;
        result = create_func(name, flags, component);
    }
    else {
        result = C2_NOT_FOUND;
    }

    MFX_DEBUG_TRACE_android_C2Error(result);
    return result;
}

void MfxC2ComponentsRegistry::RegisterMfxC2Component(const std::string& name, CreateMfxC2ComponentFunc* createFunc)
{
    MFX_DEBUG_TRACE_FUNC;

    registry_.emplace(name, createFunc);
}
