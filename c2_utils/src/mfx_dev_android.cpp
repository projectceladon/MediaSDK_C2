/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_dev_android.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_dev_android"

MfxDevAndroid::MfxDevAndroid()
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxDevAndroid::~MfxDevAndroid()
{
    MFX_DEBUG_TRACE_FUNC;
    Close();
}

mfxStatus MfxDevAndroid::Init()
{
    MFX_DEBUG_TRACE_FUNC;
    return MFX_ERR_NONE;
}

mfxStatus MfxDevAndroid::Close()
{
    MFX_DEBUG_TRACE_FUNC;
    return MFX_ERR_NONE;
}

mfxStatus MfxDevAndroid::InitMfxSession(MFXVideoSession* session)
{
    (void)session;

    MFX_DEBUG_TRACE_FUNC;
    return MFX_ERR_NONE;
}
