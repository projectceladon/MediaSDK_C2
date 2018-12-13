/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_param_storage.h"

#include "mfx_debug.h"

c2_status_t MfxC2ParamStorage::QueryParam(C2Param::Type type, C2Param** dst) const
{
    c2_status_t res = C2_OK;
    const auto const_found = const_values_.find(type);
    if (const_found != const_values_.end()) {
        auto operations_found = param_operations_.find(type);
        if (operations_found != param_operations_.end()) {
            const C2ParamOperations& operations = operations_found->second;
            if (nullptr == *dst) {
                *dst = operations.allocate_();
            }
            operations.assign_(const_found->second.get(), *dst);
        } else {
            res = C2_CORRUPTED;
        }
    } else {
        res = C2_NOT_FOUND;
    }
    return res;
}
