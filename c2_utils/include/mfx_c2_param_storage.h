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

    template<typename ParamType>
    void AddStreamInfo(const char* param_name, unsigned int stream_id,
        std::function<bool (ParamType*)> param_get);

    // Returns param values to *dst, allocates if *dst == nullptr
    c2_status_t QueryParam(C2Param::Type type, C2Param** dst) const;
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
    std::map<C2Param::Type, C2ParamOperations> param_operations_;

    std::list<std::unique_ptr<const C2Param>> const_values_;
};

template<typename ParamType>
void MfxC2ParamStorage::AddConstValue(
    const char* param_name, std::unique_ptr<ParamType>&& const_value)
{
    RegisterParam<ParamType>(param_name);

    C2ParamAllocate allocate = []() { return new ParamType(); };

    C2ParamFill fill = [src = const_value.get()] (C2Param* dst)->bool {
        *(ParamType*)dst = *src;
        return true;
    };

    // retain values during storage lifetime
    const_values_.emplace_back(std::move(const_value));

    param_operations_.emplace(ParamType::PARAM_TYPE, C2ParamOperations{allocate, std::move(fill)});
}

template<typename ParamType>
void MfxC2ParamStorage::AddStreamInfo(const char* param_name, unsigned int stream_id,
    std::function<bool (ParamType*)> param_fill)
{
    RegisterParam<ParamType>(param_name);

    C2ParamAllocate allocate = []() { return new ParamType(); };
    C2ParamFill fill = [param_fill](C2Param* dst)->bool {
        return param_fill((ParamType*)dst);
    };

    C2Param::Index index{ParamType::PARAM_TYPE};
    param_operations_.emplace(index.withStream(stream_id), C2ParamOperations{allocate, fill});
}
