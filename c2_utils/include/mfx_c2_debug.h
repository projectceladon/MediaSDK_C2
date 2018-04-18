/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_debug.h"
#include <C2Work.h>
#include <C2.h>

typedef c2_status_t android_c2_status_t;
MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(android_c2_status_t)

#if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__android_c2_status_t(_e) printf_android_c2_status_t(MFX_DEBUG_TRACE_VAR, #_e, _e)

#else // #if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__android_c2_status_t(_e)

#endif // #if MFX_DEBUG == MFX_DEBUG_YES
