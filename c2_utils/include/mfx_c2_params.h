/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Config.h>
#include <C2ParamDef.h>

enum C2ParamIndexKindVendor : uint32_t {

    kParamIndexRateControl = C2Param::TYPE_INDEX_VENDOR_START,
    kParamIndexProfile,
    kParamIndexLevel,
    kParamIndexProfileLevel,
    kParamIndexFrameQP,
    kParamIndexIntraRefresh,
    kParamIndexMemoryType,
};

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

struct C2ProfileLevelStruct {
    uint32_t profile;
    uint32_t level;

    DEFINE_AND_DESCRIBE_C2STRUCT(ProfileLevel)
    C2FIELD(profile, "Profile")
    C2FIELD(level, "Level")
};

typedef C2PortParam<C2Info, C2SimpleArrayStruct<C2ProfileLevelStruct>, kParamIndexProfileLevel> C2ProfileLevelInfo;

C2ENUM(C2MemoryType, int32_t,
    C2MemoryTypeSystem,
    C2MemoryTypeGraphics,
);

typedef C2GlobalParam<C2Setting, C2SimpleValueStruct<C2MemoryType>, kParamIndexMemoryType> C2MemoryTypeSetting;
