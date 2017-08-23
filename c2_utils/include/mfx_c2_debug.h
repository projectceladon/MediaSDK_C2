/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_debug.h"
#include <C2Work.h>
#include <C2.h>

typedef android::C2Error android_C2Error;
MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(android_C2Error)

typedef android::status_t android_status_t;
MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(android_status_t)

#if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__android_C2Error(_e) printf_android_C2Error(MFX_DEBUG_TRACE_VAR, #_e, _e)
#define MFX_DEBUG_TRACE__android_status_t(_e) printf_android_status_t(MFX_DEBUG_TRACE_VAR, #_e, _e)

#else // #if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__android_C2Error(_e)
#define MFX_DEBUG_TRACE__android_status_t(_e)

#endif // #if MFX_DEBUG == MFX_DEBUG_YES
