/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_debug.h"

#ifdef LIBVA_SUPPORT

#include <va/va.h>
MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(VAStatus)

#endif //LIBVA_SUPPORT


#if MFX_DEBUG == MFX_DEBUG_YES && defined(LIBVA_SUPPORT)

#define MFX_DEBUG_TRACE__VAStatus(_e) printf_VAStatus(MFX_DEBUG_TRACE_VAR, #_e, _e)

#else // #if MFX_DEBUG == MFX_DEBUG_YES && defined(LIBVA_SUPPORT)

#define MFX_DEBUG_TRACE__VAStatus(_e)

#endif // #if MFX_DEBUG == MFX_DEBUG_YES && defined(LIBVA_SUPPORT)
