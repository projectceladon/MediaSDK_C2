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

#pragma once

#include <C2Config.h>

enum C2ParamIndexKindVendor : C2Param::type_index_t {

    kParamIndexRateControl = C2Param::TYPE_INDEX_VENDOR_START,
    kParamIndexProfile,
    kParamIndexLevel,
    kParamIndexFrameQP,
    kParamIndexMemoryType,
};

// not existing tuning, Google defines bitrate as Info,
// so define bitrate tuning here for test purpose,
// otherwise cannot push_back it into C2Worklet::tunings
typedef C2StreamParam<C2Tuning, C2Uint32Value, kParamIndexBitrate> C2BitrateTuning;
constexpr char MFX_C2_PARAMKEY_BITRATE_TUNING[] = "coded.bitrate.tuning";

C2ENUM(C2RateControlMethod, int32_t,
    C2RateControlCQP,
    C2RateControlCBR,
    C2RateControlVBR,
);

typedef C2PortParam<C2Setting, C2SimpleValueStruct<C2RateControlMethod>, kParamIndexRateControl>::output
    C2RateControlSetting;

struct C2FrameQPStruct {
    uint32_t qp_i;
    uint32_t qp_p;
    uint32_t qp_b;

    DEFINE_AND_DESCRIBE_C2STRUCT(FrameQP)
    C2FIELD(qp_i, "QPI")
    C2FIELD(qp_p, "QPP")
    C2FIELD(qp_b, "QPB")
};

typedef C2PortParam<C2Tuning, C2FrameQPStruct, kParamIndexFrameQP>::output C2FrameQPSetting;

typedef C2PortParam<C2Tuning, C2Int32Value, kParamIndexIntraRefresh>::output C2IntraRefreshTuning;

typedef C2PortParam<C2Setting, C2Uint32Value, kParamIndexProfile>::output C2ProfileSetting;

typedef C2PortParam<C2Setting, C2Uint32Value, kParamIndexLevel>::output C2LevelSetting;

typedef C2StreamParam<C2Setting, C2FloatValue, kParamIndexFrameRate> C2FrameRateSetting;

typedef C2PortParam<C2Info, C2SimpleArrayStruct<C2ProfileLevelStruct>, kParamIndexProfileLevel> C2ProfileLevelInfo;

C2ENUM(C2MemoryType, int32_t,
    C2MemoryTypeSystem,
    C2MemoryTypeGraphics,
);

typedef C2GlobalParam<C2Setting, C2SimpleValueStruct<C2MemoryType>, kParamIndexMemoryType> C2MemoryTypeSetting;
