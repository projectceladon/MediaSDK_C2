/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_params.h"

using namespace android;

const C2ProfileLevelStruct g_h264_profile_levels[] =
{
    { PROFILE_AVC_BASELINE, LEVEL_AVC_5_1 },
    { PROFILE_AVC_MAIN, LEVEL_AVC_5_1 },
    // { PROFILE_AVC_EXTENDED, LEVEL_AVC_5_1 }, not supported by Media SDK
    { PROFILE_AVC_HIGH, LEVEL_AVC_5_1 }
};

const size_t g_h264_profile_levels_count = MFX_GET_ARRAY_SIZE(g_h264_profile_levels);

const C2ProfileLevelStruct g_h265_profile_levels[] =
{
    { PROFILE_HEVC_MAIN, LEVEL_HEVC_MAIN_5_1 },
    /*{ PROFILE_HEVC_MAIN_10, LEVEL_HEVC_MAIN_5_1 }, supports only 8 bit (see caps.BitDepth8Only)*/
};

const size_t g_h265_profile_levels_count = MFX_GET_ARRAY_SIZE(g_h265_profile_levels);
