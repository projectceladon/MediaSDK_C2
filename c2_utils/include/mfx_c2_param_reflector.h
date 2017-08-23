/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Param.h>
#include <C2Work.h>
#include <map>

class MfxC2ParamReflector
{
private:
    std::vector<std::shared_ptr<android::C2ParamDescriptor>> params_descriptors_;

    std::map<android::C2Param::BaseIndex, android::C2StructDescriptor> params_struct_descriptors_;

    std::map<android::C2ParamField, android::C2FieldSupportedValues> params_supported_values_;

public:
    template<typename ParamType>
    void RegisterParam(const char* param_name);

    template<typename ParamType, typename ValueType, typename FieldType>
    void RegisterSupportedRange(FieldType ValueType::* pm, FieldType min, FieldType max);

    bool ValidateParam(const android::C2Param* param,
        std::vector<std::unique_ptr<android::C2SettingResult>>* const failures);

    std::unique_ptr<android::C2SettingResult> FindParam(
        const android::C2Param* param);

    status_t getSupportedParams(
        std::vector<std::shared_ptr<android::C2ParamDescriptor>>* const params) const;
};

template<typename ParamType>
void MfxC2ParamReflector::RegisterParam(const char* param_name)
{
    using namespace android;

    params_descriptors_.push_back(
        std::make_shared<C2ParamDescriptor>(false, param_name, ParamType::typeIndex));

    C2Param::BaseIndex base_index = C2Param::Type(ParamType::typeIndex).paramIndex();
    params_struct_descriptors_.insert({ base_index, C2StructDescriptor(base_index, ParamType::fieldList) });
};

template<typename ParamType, typename ValueType, typename FieldType>
void MfxC2ParamReflector::RegisterSupportedRange(FieldType ValueType::* pm, FieldType min, FieldType max)
{
    using namespace android;

    ParamType temp_param; // C2ParamField constructor demands pointer to instance

    C2ParamField field(&temp_param, pm);
    C2FieldSupportedValues values(min, max);

    params_supported_values_.emplace(field, values);
};
