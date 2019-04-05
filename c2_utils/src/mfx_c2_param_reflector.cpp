/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_param_reflector.h"
#include "mfx_debug.h"
#include "mfx_c2_utils.h"
#include <sstream>

// Google uses a trick to access private fields/methods of some classes
// like C2ParamField and C2FieldDescriptor: it declares _C2ParamInspector
// as friend for those private classes, then define _C2ParamInspector in
// internal header - C2ParamInternal.h.
// Unfortunately Google's _C2ParamInspector is not enough for out needs:
// it gives access to C2ParamField, but doesn't help with C2FieldDescriptor.
// So we have to define our own _C2ParamInspector for everything we need.
struct _C2ParamInspector
{
   inline static uint32_t getIndex(const C2ParamField &pf) {
        return pf._mIndex;
    }

    inline static uint32_t getOffset(const C2ParamField &pf) {
        return pf._mFieldId._mOffset;
    }

    inline static uint32_t getSize(const C2ParamField &pf) {
        return pf._mFieldId._mSize;
    }

    inline static _C2FieldId getFieldId(const C2ParamField &pf) {
        return pf._mFieldId;
    }

    inline static
    C2ParamField CreateParamField(C2Param::Index index, uint32_t offset, uint32_t size) {
        return C2ParamField(index, offset, size);
    }
    // here and below is what Google is missing
    inline static uint32_t getOffset(const C2FieldDescriptor& fd) {
        return fd._mFieldId._mOffset;
    }

    inline static uint32_t getSize(const C2FieldDescriptor& fd) {
        return fd._mFieldId._mSize;
    }
};

inline size_t GetSize(C2FieldDescriptor::type_t type)
{
    switch (type) {
        case C2FieldDescriptor::INT32: {
            return sizeof(int32_t);
        }
        case C2FieldDescriptor::UINT32: {
            return sizeof(uint32_t);
        }
        case C2FieldDescriptor::FLOAT: {
            return sizeof(float);
        }
        default: // other constraints not supported yet
            return 0;
    }
}

inline bool CheckRange(const C2FieldSupportedValues& supported, C2FieldDescriptor::type_t type, const uint8_t* ptr_val)
{
    if(supported.type != C2FieldSupportedValues::RANGE)
        return false;
    switch (type) {
        case C2FieldDescriptor::INT32: {
            int32_t min = supported.range.min.i32;
            int32_t max = supported.range.max.i32;
            const int32_t val = *(const int32_t*)ptr_val;

            if(val < min || val > max) {
                return false;
            }
            break;
        }
        case C2FieldDescriptor::UINT32: {
            uint32_t min = supported.range.min.u32;
            uint32_t max = supported.range.max.u32;
            const uint32_t val = *(const uint32_t*)ptr_val;

            if(val < min || val > max) {
                return false;
            }
            break;
        }
        case C2FieldDescriptor::FLOAT: {
            float min = supported.range.min.fp;
            float max = supported.range.max.fp;
            const float val = *(const float*)ptr_val;

            if(val < min || val > max) {
                return false;
            }
            break;
        }
        default: // other constraints not supported yet
            return false;
    }
    return true;
}

std::unique_ptr<C2StructDescriptor> MfxC2ParamReflector::describe(
    C2Param::CoreIndex coreIndex) const
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_STREAM(std::hex << NAMED(coreIndex.coreIndex()));

    std::unique_ptr<C2StructDescriptor> result;

    auto found_struct = params_struct_descriptors_.find(C2Param::Type(coreIndex.coreIndex()));
    if(found_struct != params_struct_descriptors_.end()) {
        result = std::make_unique<C2StructDescriptor>(found_struct->second);
    }

    return result;
}

bool MfxC2ParamReflector::ValidateParam(const C2Param* param,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    bool res = true;

    do {

        C2Param::Type param_type = param->type();

        MFX_DEBUG_TRACE_STREAM(std::hex << NAMED(param_type.type()));
        auto found_struct = params_struct_descriptors_.find(param_type);
        if(found_struct == params_struct_descriptors_.end()) {
            MFX_DEBUG_TRACE_MSG("the whole param is not supported");
            res = false;
            failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
            break;
        }

        const C2StructDescriptor& struct_desc = found_struct->second;
        for(auto it_field = struct_desc.cbegin(); it_field != struct_desc.cend(); ++it_field) {

            const C2FieldDescriptor& field_desc = *it_field;
            MFX_DEBUG_TRACE_STREAM(std::hex << NAMED((uint32_t)field_desc.type()));

            if(field_desc.type() == C2FieldDescriptor::INT32 ||
               field_desc.type() == C2FieldDescriptor::UINT32 ||
               field_desc.type() == C2FieldDescriptor::FLOAT) {

                // C2ParamField measures offset in C2Param structure,
                // C2FieldDescriptor, in turn, in embedded data struct.
                // So sizeof(C2Param) should be added to offset before search.

                C2ParamField param_field = _C2ParamInspector::CreateParamField(param->index(),
                    _C2ParamInspector::getOffset(field_desc) + sizeof(C2Param), GetSize(field_desc.type()));

                MFX_DEBUG_TRACE_STREAM("Looking for C2ParamField:"
                    << std::hex << " index: " << _C2ParamInspector::getIndex(param_field)
                    << std::dec << " offset: " << _C2ParamInspector::getOffset(param_field)
                    << " size: " << _C2ParamInspector::getSize(param_field));

                auto it_supported = params_supported_values_.find(param_field);
                if(it_supported != params_supported_values_.end()) {
                   const C2FieldSupportedValues& supported = it_supported->second;
                   switch(supported.type) {
                        case C2FieldSupportedValues::RANGE: {
                           const uint8_t* ptr_val = (const uint8_t*)param
                               + sizeof(C2Param) + _C2ParamInspector::getOffset(field_desc);
                            if(!CheckRange(supported, field_desc.type(), ptr_val)) {
                               res = false;
                               failures->push_back(MakeC2SettingResult(param_field,
                                   C2SettingResult::BAD_VALUE, {}, &supported));
                            }
                            break;
                        }
                        default: // other constraints not supported yet
                            res = false;
                            failures->push_back(MakeC2SettingResult(param_field,
                               C2SettingResult::BAD_TYPE));
                            break;
                   }
                }

            } else {
                MFX_DEBUG_TRACE_MSG("other types not supported yet");
                res = false;
                failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
            }
        }


    } while(false);

    MFX_DEBUG_TRACE_STREAM(NAMED(res));
    return res;
}

bool MfxC2ParamReflector::FindParam(C2Param::Index param_index) const
{
    return FindC2Param(params_descriptors_, param_index);
}

std::unique_ptr<C2SettingResult> MfxC2ParamReflector::FindParam(const C2Param* param) const
{
    return FindC2Param(params_descriptors_, param);
}

c2_status_t MfxC2ParamReflector::getSupportedParams(
    std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const
{
    MFX_DEBUG_TRACE_FUNC;

    params->clear();
    std::copy_if(params_descriptors_.begin(), params_descriptors_.end(),
        std::back_inserter(*params),
        [](const auto& p) { return p->index().isVendor(); }
    );

    return C2_OK;
}

c2_status_t MfxC2ParamReflector::querySupportedValues_vb(
        std::vector<C2FieldSupportedValuesQuery> &queries, c2_blocking_t mayBlock) const
{
    MFX_DEBUG_TRACE_FUNC;

    (void)mayBlock;

    for (C2FieldSupportedValuesQuery &query : queries) {
        std::ostringstream oss;

        const auto iter = std::find_if(params_supported_values_.begin(), params_supported_values_.end(),
            [query](const std::pair<C2ParamField, C2FieldSupportedValues>& p){ return (_C2ParamInspector::getIndex(p.first) & _C2ParamInspector::getIndex(query.field()) & 0xFFF) &&
                                                                                      (_C2ParamInspector::getFieldId(p.first) == _C2ParamInspector::getFieldId(query.field())); });

        if (iter == params_supported_values_.end()) {
            oss << "bad field:"
                << " ParamID:" << std::hex << _C2ParamInspector::getIndex(query.field())
                << " offset:" << std::dec << _C2ParamInspector::getOffset(query.field())
                << " size:" << _C2ParamInspector::getSize(query.field()) << "\n";
            query.status = C2_NOT_FOUND;
            MFX_DEBUG_TRACE_MSG(oss.str().c_str());
            continue;
        }

        switch (query.type()) {
        case C2FieldSupportedValuesQuery::CURRENT:
        case C2FieldSupportedValuesQuery::POSSIBLE:
            query.values = iter->second;
            query.status = C2_OK;
            break;
        default:
            oss << "bad query type: " << query.type() << "\n";
            query.status = C2_BAD_VALUE;
        }

        MFX_DEBUG_TRACE_MSG(oss.str().c_str());
    }

    // function always returns C2_OK, errors pass trough query.status - align behavior with
    // current Google's implementaion: C2InterfaceHelper::querySupportedValues
    return C2_OK;
}

#if MFX_DEBUG == MFX_DEBUG_YES

static std::ostream& operator<<(std::ostream& os, C2FieldDescriptor::type_t type)
{
    const char* type_names[] = { "NO_INIT", "INT32", "UINT32", "INT64", "UINT64", "FLOAT", };

    uint32_t index = (uint32_t)type;

    if(index < MFX_GET_ARRAY_SIZE(type_names)) {
        os << type_names[index];
    } else {
        os << "UNKNOWN";
    }
    return os;
}

static std::ostream& operator<<(std::ostream& os, C2FieldSupportedValues::type_t type)
{
    const char* type_names[] = { "RANGE", "VALUES", "FLAGS", };

    uint32_t index = (uint32_t)type;

    if(index < MFX_GET_ARRAY_SIZE(type_names)) {
        os << type_names[index];
    } else {
        os << "UNKNOWN";
    }
    return os;
}

void MfxC2ParamReflector::DumpParams()
{
    MFX_DEBUG_TRACE_FUNC;

    const std::string indent(4, ' ');

    for(auto desc : params_descriptors_) {
        std::ostringstream oss;
        uint32_t index = desc->index();
        oss << std::hex << index << " " << desc->name();
        MFX_DEBUG_TRACE_MSG(oss.str().c_str());
    }

    MFX_DEBUG_TRACE_MSG("params_struct_descriptors_");
    for(const auto& pair : params_struct_descriptors_) {
        std::ostringstream oss;
        oss << std::hex << *(uint32_t*)&pair.first;
        MFX_DEBUG_TRACE_MSG(oss.str().c_str());

        for(const auto& field_desc : pair.second) {
            std::ostringstream oss;
            oss << indent << field_desc.name() << " " << field_desc.type()
                << " " << _C2ParamInspector::getOffset(field_desc)
                << " " << _C2ParamInspector::getSize(field_desc);
            MFX_DEBUG_TRACE_MSG(oss.str().c_str());
        }
    }

    MFX_DEBUG_TRACE_MSG("params_supported_values_");
    for(const auto& pair : params_supported_values_) {
        std::ostringstream oss;

        const C2ParamField& param_field = pair.first;

        oss << "ParamID:" << std::hex << _C2ParamInspector::getIndex(param_field);
        oss << " offset:" << std::dec << _C2ParamInspector::getOffset(param_field);
        oss << " size:" << _C2ParamInspector::getSize(param_field);

        const C2FieldSupportedValues& values = pair.second;

        oss << " " << values.type;
        if(values.type == C2FieldSupportedValues::RANGE) {

            oss << std::dec << " [" << values.range.min.u32 << "-" << values.range.max.u32 << "]";
        }
        MFX_DEBUG_TRACE_MSG(oss.str().c_str());
    }
}

#endif // MFX_DEBUG == MFX_DEBUG_YES
