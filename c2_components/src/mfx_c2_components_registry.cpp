// Copyright (c) 2017-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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
#include "mfx_c2_secure_decoder_component.h"
#endif

using namespace android;


#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_components_registry"

extern "C" EXPORT MfxC2Component* MfxCreateC2Component(
    const char* name,
    const MfxC2Component::CreateConfig& config,
    std::shared_ptr<C2ReflectorHelper> reflector,
    c2_status_t* status)
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2Component* component {};
    c2_status_t res =
        MfxC2ComponentsRegistry::getInstance().CreateMfxC2Component(name, config,
            std::move(reflector),  &component);
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
    MfxC2SecureDecoderComponent::RegisterClass(*this);
#endif
}

MfxC2ComponentsRegistry& MfxC2ComponentsRegistry::getInstance()
{
    MFX_DEBUG_TRACE_FUNC;

    static MfxC2ComponentsRegistry g_registry;
    return g_registry;
}

c2_status_t MfxC2ComponentsRegistry::CreateMfxC2Component(const char* name,
    const MfxC2Component::CreateConfig& config,
    std::shared_ptr<C2ReflectorHelper> reflector, MfxC2Component** component)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t result = C2_OK;
    *component = nullptr;

    MFX_DEBUG_TRACE_I32(m_registry.size());

    auto it = m_registry.find(name);
    if(it != m_registry.end()) {
        CreateMfxC2ComponentFunc* create_func = it->second;
        *component = create_func(name, config, std::move(reflector), &result);
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

    m_registry.emplace(name, createFunc);
}
