/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Component.h>
#include "mfx_c2_params.h"
#include "mfx_c2_param_reflector.h"

#define MFX_C2_COMPONENT_STORE_NAME "MfxC2ComponentStore"

#define CREATE_MFX_C2_COMPONENT_FUNC_NAME "MfxCreateC2Component"

#define MFX_C2_CONFIG_FILE_NAME "mfx_c2_store.conf"
#define MFX_C2_CONFIG_FILE_PATH "/vendor/etc"

#define MFX_C2_CONFIG_XML_FILE_NAME "media_codecs_intel_c2_video.xml"
#define MFX_C2_CONFIG_XML_FILE_PATH "/vendor/etc"

#define MFX_C2_DUMP_DIR "/data/local/tmp"
#define MFX_C2_DUMP_OUTPUT_SUB_DIR "c2-output"

const c2_nsecs_t MFX_SECOND_NS = 1000000000; // 1e9

extern const size_t g_h264_profile_levels_count;
extern const C2ProfileLevelStruct g_h264_profile_levels[];

extern const size_t g_h265_profile_levels_count;
extern const C2ProfileLevelStruct g_h265_profile_levels[];

// TODO: Update this value if you need to add ExtBufHolder type
constexpr uint16_t g_max_num_ext_buffers = 2;
