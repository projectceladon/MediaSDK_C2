/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_encoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"

using namespace android;

MfxC2EncoderComponent::MfxC2EncoderComponent(const android::C2String name, int flags, EncoderType encoder_type) :
    MfxC2Component(name, flags), encoder_type_(encoder_type)
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2EncoderComponent::~MfxC2EncoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    session_.Close();
}

void MfxC2EncoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("C2.h264ve",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H264>);
}

android::status_t MfxC2EncoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MfxDev::Create(&device_);
    if(mfx_res == MFX_ERR_NONE) {
        mfx_res = session_.Init(MFX_IMPLEMENTATION, &g_required_mfx_version);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Major);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Minor);

        if(mfx_res == MFX_ERR_NONE) {
            mfxIMPL mfx_impl {};
            mfxStatus sts = session_.QueryIMPL(&mfx_impl);
            MFX_DEBUG_TRACE_mfxStatus(sts);
            MFX_DEBUG_TRACE_I32(mfx_impl);

            encoder_.reset(MFX_NEW_NO_THROW(MFXVideoENCODE(session_)));
            if(encoder_ != nullptr) {
                mfx_res = device_->InitMfxSession(&session_);
            }
            else {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        } else {
            MFX_DEBUG_TRACE_MSG("MFXVideoSession::Init failed");
            MFX_DEBUG_TRACE_mfxStatus(mfx_res);
        }
    }

    return MfxStatusToC2(mfx_res);
}
