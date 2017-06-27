/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MFX_C2_DEFS_H__
#define __MFX_C2_DEFS_H__

#include <C2Component.h>

class MfxC2Component;

typedef android::status_t (CreateMfxC2ComponentFunc)(const char* name, int flags,
    MfxC2Component** component);

#define CREATE_MFX_C2_COMPONENT_FUNC_NAME "MfxCreateC2Component"

#define MFX_C2_CONFIG_FILE_NAME "mfx_c2_store.conf"
#define MFX_C2_CONFIG_FILE_PATH "/etc"

const nsecs_t MFX_SECOND_NS = 1000000000; // 1e9

#endif // #ifndef __MFX_C2_DEFS_H__
