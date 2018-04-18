/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_params.h"
#include "mfx_legacy_defs.h"

using namespace android;

const C2ProfileLevelStruct g_h264_profile_levels[] =
{
    { LEGACY_VIDEO_AVCProfileBaseline, LEGACY_VIDEO_AVCLevel51 },
    { LEGACY_VIDEO_AVCProfileMain, LEGACY_VIDEO_AVCLevel51 },
    // { LEGACY_VIDEO_AVCProfileExtended, LEGACY_VIDEO_AVCLevel51 }, not supported by Media SDK
    { LEGACY_VIDEO_AVCProfileHigh, LEGACY_VIDEO_AVCLevel51 }
};

const size_t g_h264_profile_levels_count = MFX_GET_ARRAY_SIZE(g_h264_profile_levels);
