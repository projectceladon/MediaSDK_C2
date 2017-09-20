/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Config.h>
#include <C2ParamDef.h>

namespace android {

enum C2ParamIndexKindVendor : uint32_t {

    kParamIndexRateControl = C2Param::BaseIndex::kVendorStart,
    kParamIndexBitrate,
    kParamIndexFrameQP,
    kParamIndexIntraRefresh,
};

C2ENUM(C2RateControlMethod, int32_t,
    C2RateControlCQP,
    C2RateControlCBR,
    C2RateControlVBR,
);

typedef C2PortParam<C2Setting, C2SimpleValueStruct<C2RateControlMethod>, kParamIndexRateControl>::output
    C2RateControlSetting;

typedef C2PortParam<C2Tuning, C2Uint32Value, kParamIndexBitrate>::output C2BitrateTuning;

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

} // namespace android
