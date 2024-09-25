// Copyright (c) 2017-2024 Intel Corporation
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

#include "mfx_c2_secure_decoder_component.h"

#include "mfx_defs.h"
#include "mfx_c2_utils.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_defaults.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_secure_decoder_component"

constexpr c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;


MfxC2SecureDecoderComponent::MfxC2SecureDecoderComponent(const C2String name, const CreateConfig& config,
        std::shared_ptr<C2ReflectorHelper> reflector, DecoderType decoder_type) :
        MfxC2DecoderComponent(name, config, std::move(reflector), decoder_type)
{
    MFX_DEBUG_TRACE_FUNC;
    m_secure = true;

    addParameter(
        DefineParam(m_secureMode, C2_PARAMKEY_SECURE_MODE)
        .withConstValue(new C2SecureModeTuning(C2Config::secure_mode_t::SM_READ_PROTECTED_WITH_ENCRYPTED))
        .build());
}

MfxC2SecureDecoderComponent::~MfxC2SecureDecoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2DecoderComponent::Release();
}

void MfxC2SecureDecoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("c2.intel.avc.decoder.secure",
        &MfxC2Component::Factory<MfxC2SecureDecoderComponent, DecoderType>::Create<DECODER_H264_SECURE>);

    registry.RegisterMfxC2Component("c2.intel.hevc.decoder.secure",
        &MfxC2Component::Factory<MfxC2SecureDecoderComponent, DecoderType>::Create<DECODER_H265_SECURE>);
}

