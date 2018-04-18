/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Component.h>
#include "mfx_c2_params.h"

class MfxC2Component;

typedef MfxC2Component* (CreateMfxC2ComponentFunc)(const char* name, int flags,
    c2_status_t* status);

#define MFX_C2_COMPONENT_STORE_NAME "MfxC2ComponentStore"

#define CREATE_MFX_C2_COMPONENT_FUNC_NAME "MfxCreateC2Component"

#define MFX_C2_CONFIG_FILE_NAME "mfx_c2_store.conf"
#define MFX_C2_CONFIG_FILE_PATH "/etc"

const c2_nsecs_t MFX_SECOND_NS = 1000000000; // 1e9

extern const size_t g_h264_profile_levels_count;
extern const C2ProfileLevelStruct g_h264_profile_levels[];
