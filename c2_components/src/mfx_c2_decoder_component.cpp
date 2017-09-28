/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_decoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_decoder_component"

MfxC2DecoderComponent::MfxC2DecoderComponent(const android::C2String name, int flags, DecoderType decoder_type) :
    MfxC2Component(name, flags), decoder_type_(decoder_type)
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2DecoderComponent::~MfxC2DecoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    session_.Close();
}

void MfxC2DecoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("C2.h264vd",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_H264>);
}

android::status_t MfxC2DecoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MfxDev::Create(&device_);
    if(mfx_res == MFX_ERR_NONE) {
        mfx_res = session_.Init(mfx_implementation_, &g_required_mfx_version);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Major);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Minor);

        if(mfx_res == MFX_ERR_NONE) {
            mfxStatus sts = session_.QueryIMPL(&mfx_implementation_);
            MFX_DEBUG_TRACE__mfxStatus(sts);
            MFX_DEBUG_TRACE_I32(mfx_implementation_);

            decoder_.reset(MFX_NEW_NO_THROW(MFXVideoDECODE(session_)));
            if(decoder_ != nullptr) {
                mfx_res = device_->InitMfxSession(&session_);
            }
            else {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        } else {
            MFX_DEBUG_TRACE_MSG("MFXVideoSession::Init failed");
            MFX_DEBUG_TRACE__mfxStatus(mfx_res);
        }
    }

    return MfxStatusToC2(mfx_res);
}
