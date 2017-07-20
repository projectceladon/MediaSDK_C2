/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2ParamDef.h>

namespace android {

enum C2ParamIndexKindVendor : uint32_t {

    kParamIndexRateControl = C2Param::BaseIndex::kVendorStart,
    kParamIndexBitrate,
};

C2ENUM(C2RateControlMethod, int32_t,
    C2RateControlCQP,
    C2RateControlCBR,
);

typedef C2PortParam<C2Setting, C2SimpleValueStruct<C2RateControlMethod>, kParamIndexRateControl>::output
    C2RateControlSetting;

typedef C2PortParam<C2Tuning, C2Int32Value, kParamIndexBitrate>::output C2BitrateTuning;

} // namespace android
