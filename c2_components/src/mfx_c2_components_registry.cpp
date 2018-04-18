/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_components_registry.h"

#include "mfx_debug.h"
#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_component.h"

#ifdef MOCK_COMPONENTS
#include "mfx_c2_mock_component.h"
#else
#include "mfx_c2_decoder_component.h"
#include "mfx_c2_encoder_component.h"
#endif

using namespace android;


#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_components_registry"

extern "C" EXPORT MfxC2Component* MfxCreateC2Component(
    const char* name,
    int flags,
    c2_status_t* status)
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2Component* component {};
    c2_status_t res =
        MfxC2ComponentsRegistry::getInstance().CreateMfxC2Component(name, flags, &component);
    if (nullptr != status) *status = res;
    return component;
}

MfxC2ComponentsRegistry::MfxC2ComponentsRegistry()
{
    // Here should be list of calls like this:
    // MfxC2SomeComponent::RegisterClass();
    // Auto-registration with global variables might not work within static libraries
#ifdef MOCK_COMPONENTS
    MfxC2MockComponent::RegisterClass(*this);
#else
    MfxC2DecoderComponent::RegisterClass(*this);
    MfxC2EncoderComponent::RegisterClass(*this);
#endif
}

MfxC2ComponentsRegistry& MfxC2ComponentsRegistry::getInstance()
{
    MFX_DEBUG_TRACE_FUNC;

    static MfxC2ComponentsRegistry g_registry;
    return g_registry;
}

c2_status_t MfxC2ComponentsRegistry::CreateMfxC2Component(const char* name, int flags, MfxC2Component** component)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t result = C2_OK;
    *component = nullptr;

    MFX_DEBUG_TRACE_I32(registry_.size());

    auto it = registry_.find(name);
    if(it != registry_.end()) {
        CreateMfxC2ComponentFunc* create_func = it->second;
        *component = create_func(name, flags, &result);
    }
    else {
        result = C2_NOT_FOUND;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(result);
    return result;
}

void MfxC2ComponentsRegistry::RegisterMfxC2Component(const std::string& name, CreateMfxC2ComponentFunc* createFunc)
{
    MFX_DEBUG_TRACE_FUNC;

    registry_.emplace(name, createFunc);
}
