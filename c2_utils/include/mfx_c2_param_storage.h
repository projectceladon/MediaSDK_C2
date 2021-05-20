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
    void AddValue(const char* param_name, std::unique_ptr<ParamType>&& value);

    template<typename ParamType>
    c2_status_t UpdateValue(C2Param::Index param_index, std::unique_ptr<ParamType>&& value);

    template<typename ParamType>
    void AddStreamInfo(const char* param_name, unsigned int stream_id,
        std::function<bool (ParamType*)> param_get,
        std::function<bool (const ParamType&)> param_set = {});

    // Returns param values to *dst, allocates if *dst == nullptr
    c2_status_t QueryParam(C2Param::Index index, C2Param** dst) const;

    c2_status_t ConfigParam(const C2Param& src, bool component_stopped,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures);


#if MFX_DEBUG == MFX_DEBUG_YES
    void DumpParams();
#else
    void DumpParams() { }
#endif

private:
    // Operations specified for any parameter type
    typedef std::function<C2Param*()> C2ParamAllocate;
    typedef std::function<bool(C2Param* dst)> C2ParamGet;
    typedef std::function<bool(const C2Param& src)> C2ParamSet;

    struct C2ParamOperations
    {
        C2ParamAllocate allocate_;
        C2ParamGet get_;
        C2ParamSet set_;
    };

private:
    std::shared_ptr<MfxC2ParamReflector> reflector_;

    std::vector<std::shared_ptr<C2ParamDescriptor>> params_descriptors_;

    std::map<C2ParamField, C2FieldSupportedValues> params_supported_values_;

    std::map<C2Param::Index, C2ParamOperations> param_operations_;

    std::map<C2Param::Index, std::unique_ptr<const C2Param>> values_;

    mutable std::mutex values_mutex_;
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
void MfxC2ParamStorage::AddValue(
    const char* param_name, std::unique_ptr<ParamType>&& value)
{
    RegisterParam<ParamType>(param_name);

    C2Param::Index index = C2Param::Index(value->index());

    {
        std::lock_guard<std::mutex> lock(values_mutex_);
        values_.emplace(index, std::move(value));
    }
}

template<typename ParamType>
c2_status_t MfxC2ParamStorage::UpdateValue(C2Param::Index param_index, std::unique_ptr<ParamType>&& value)
{
    {
        std::lock_guard<std::mutex> lock(values_mutex_);
        auto found = values_.find(param_index);

        if (found != values_.end()) {
            found->second = std::move(value);
            return C2_OK;
        }
    }
    return C2_NOT_FOUND;
}

template<typename ParamType>
void MfxC2ParamStorage::AddStreamInfo(const char* param_name, unsigned int stream_id,
    std::function<bool (ParamType*)> param_get,
    std::function<bool (const ParamType&)> param_set)
{
    RegisterParam<ParamType>(param_name);

    C2ParamAllocate allocate = [stream_id]() {
        ParamType* res = new ParamType();
        res->setStream(stream_id); // compiled if ParamType is C2StreamParam
        return res;
    };

    C2ParamGet get_function = [param_get](C2Param* dst)->bool {
        return param_get((ParamType*)dst);
    };

    C2ParamSet set_function = [param_set](const C2Param& src)->bool {
        return param_set(*(const ParamType*)&src);
    };

    C2Param::Index index{ParamType::PARAM_TYPE};
    param_operations_.emplace(index.withStream(stream_id),
        C2ParamOperations{allocate, get_function, set_function});
}
