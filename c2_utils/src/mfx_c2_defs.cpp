// Copyright (c) 2017-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "mfx_defs.h"
#include "mfx_c2_defs.h"

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
