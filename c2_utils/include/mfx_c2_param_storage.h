/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_c2_param_reflector.h"

class MfxC2ParamStorage : public MfxC2ParamReflector
{
public:
    template<typename ParamType>
    void AddConstValue(const char* param_name, std::unique_ptr<ParamType>&& const_value);
    // Returns param values to *dst, allocates if *dst == nullptr
    c2_status_t QueryParam(C2Param::Type type, C2Param** dst) const;
private:
    // Operations specified for any parameter type
    typedef std::function<C2Param*()> C2ParamAllocate;
    typedef std::function<void(const C2Param* src, C2Param* dst)> C2ParamAssign;

    struct C2ParamOperations
    {
        C2ParamAllocate allocate_;
        C2ParamAssign assign_;
    };

private:
    std::map<C2Param::Type, C2ParamOperations> param_operations_;

    std::map<C2Param::Type, std::unique_ptr<C2Param>> const_values_;
};

template<typename ParamType>
void MfxC2ParamStorage::AddConstValue(
    const char* param_name, std::unique_ptr<ParamType>&& const_value)
{
    RegisterParam<ParamType>(param_name);
    const_values_.emplace(ParamType::PARAM_TYPE, std::move(const_value));

    C2ParamAllocate allocate = []() { return new ParamType(); };
    C2ParamAssign assign = [](const C2Param* src, C2Param* dst) {
        *(ParamType*)dst = *(const ParamType*)src;
    };

    param_operations_.emplace(ParamType::PARAM_TYPE, C2ParamOperations{allocate, assign});
}
