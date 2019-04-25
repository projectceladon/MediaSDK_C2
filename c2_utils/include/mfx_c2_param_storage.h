/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_c2_param_reflector.h"

class MfxC2ParamStorage
{
public:
    MfxC2ParamStorage(std::shared_ptr<MfxC2ParamReflector> reflector):
        reflector_(std::move(reflector)) {}

    template<typename ParamType>
    void RegisterParam(const char* param_name);

    template<typename ParamType, typename ValueType, typename FieldType>
    void RegisterSupportedRange(FieldType ValueType::* pm, FieldType min, FieldType max);

    template<typename ParamType, typename ValueType, typename FieldType>
    void RegisterSupportedValues(FieldType ValueType::* pm, const std::vector<FieldType> &supported_values);

    bool ValidateParam(const C2Param* param,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures);

    std::unique_ptr<C2SettingResult> FindParam(
        const C2Param* param) const;

    bool FindParam(C2Param::Index param_index) const;

    c2_status_t getSupportedParams(
        std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const;

    c2_status_t querySupportedValues_vb(
        std::vector<C2FieldSupportedValuesQuery> &queries, c2_blocking_t mayBlock) const;

    template<typename ParamType>
    void AddConstValue(const char* param_name, std::unique_ptr<ParamType>&& const_value);

    template<typename ParamType>
    void AddStreamInfo(const char* param_name, unsigned int stream_id,
        std::function<bool (ParamType*)> param_get);

    // Returns param values to *dst, allocates if *dst == nullptr
    c2_status_t QueryParam(C2Param::Index index, C2Param** dst) const;

#if MFX_DEBUG == MFX_DEBUG_YES
    void DumpParams();
#else
    void DumpParams() { }
#endif

private:
    // Operations specified for any parameter type
    typedef std::function<C2Param*()> C2ParamAllocate;
    typedef std::function<bool(C2Param* dst)> C2ParamFill;

    struct C2ParamOperations
    {
        C2ParamAllocate allocate_;
        C2ParamFill fill_;
    };

private:
    std::shared_ptr<MfxC2ParamReflector> reflector_;

    std::vector<std::shared_ptr<C2ParamDescriptor>> params_descriptors_;

    std::map<C2ParamField, C2FieldSupportedValues> params_supported_values_;

    std::map<C2Param::Index, C2ParamOperations> param_operations_;

    std::map<C2Param::Index, std::unique_ptr<const C2Param>> const_values_;
};

template<typename ParamType>
void MfxC2ParamStorage::RegisterParam(const char* param_name)
{
    using namespace android;

    params_descriptors_.push_back(
        std::make_shared<C2ParamDescriptor>(false, param_name, ParamType::PARAM_TYPE));

    reflector_->AddDescription<ParamType>();

};

template<typename ParamType, typename ValueType, typename FieldType>
void MfxC2ParamStorage::RegisterSupportedRange(FieldType ValueType::* pm, FieldType min, FieldType max)
{
    using namespace android;

    ParamType temp_param; // C2ParamField constructor demands pointer to instance

    C2ParamField field(&temp_param, pm);
    C2FieldSupportedValues values(min, max);

    params_supported_values_.emplace(field, values);
};

template<typename ParamType, typename ValueType, typename FieldType>
void MfxC2ParamStorage::RegisterSupportedValues(FieldType ValueType::* pm, const std::vector<FieldType> &supported_values)
{
    using namespace android;

    ParamType temp_param;

    C2ParamField field(&temp_param, pm);
    C2FieldSupportedValues values(false, supported_values);

    params_supported_values_.emplace(field, values);
};

template<typename ParamType>
void MfxC2ParamStorage::AddConstValue(
    const char* param_name, std::unique_ptr<ParamType>&& const_value)
{
    RegisterParam<ParamType>(param_name);

    C2Param::Index index = C2Param::Index(const_value->index());

    const_values_.emplace(index, std::move(const_value));
}

template<typename ParamType>
void MfxC2ParamStorage::AddStreamInfo(const char* param_name, unsigned int stream_id,
    std::function<bool (ParamType*)> param_fill)
{
    RegisterParam<ParamType>(param_name);

    C2ParamAllocate allocate = [stream_id]() {
        ParamType* res = new ParamType();
        res->setStream(stream_id); // compiled if ParamType is C2StreamParam
        return res;
    };
    C2ParamFill fill = [param_fill](C2Param* dst)->bool {
        return param_fill((ParamType*)dst);
    };

    C2Param::Index index{ParamType::PARAM_TYPE};
    param_operations_.emplace(index.withStream(stream_id), C2ParamOperations{allocate, fill});
}
