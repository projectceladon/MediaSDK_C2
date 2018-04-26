/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

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

bool MfxC2ParamReflector::ValidateParam(const C2Param* param,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    bool res = true;

    do {

        C2Param::CoreIndex base_index = C2Param::Type(param->type()).typeIndex();
        auto found_struct = params_struct_descriptors_.find(base_index);
        if(found_struct == params_struct_descriptors_.end()) {
            // the whole param is not supported
            res = false;
            failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
            break;
        }

        const C2StructDescriptor& struct_desc = found_struct->second;
        for(auto it_field = struct_desc.cbegin(); it_field != struct_desc.cend(); ++it_field) {

            const C2FieldDescriptor& field_desc = *it_field;
            switch(field_desc.type()) {
                case C2FieldDescriptor::UINT32: {

                    // C2ParamField measures offset in C2Param structure,
                    // C2FieldDescriptor, in turn, in embedded data struct.
                    // So sizeof(C2Param) should be added to offset before search.

                    C2ParamField param_field = _C2ParamInspector::CreateParamField(param->index(),
                        _C2ParamInspector::getOffset(field_desc) + sizeof(C2Param), sizeof(uint32_t));

                    MFX_DEBUG_TRACE_STREAM("Looking for C2ParamField:"
                        << std::hex << " index: " << _C2ParamInspector::getIndex(param_field)
                        << std::dec << " offset: " << _C2ParamInspector::getOffset(param_field)
                        << " size: " << _C2ParamInspector::getSize(param_field));

                    auto it_supported = params_supported_values_.find(param_field);
                    if(it_supported != params_supported_values_.end()) {
                        const C2FieldSupportedValues& supported = it_supported->second;
                        switch(supported.type) {
                            case C2FieldSupportedValues::RANGE: {
                                uint32_t min = supported.range.min.u32;
                                uint32_t max = supported.range.max.u32;
                                const uint8_t* ptr_val = (const uint8_t*)param
                                    + sizeof(C2Param) + _C2ParamInspector::getOffset(field_desc);
                                const uint32_t val = *(const uint32_t*)ptr_val;

                                if(val < min || val > max) {
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

                    break;
                }
                default: // other types not supported yet
                    res = false;
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
                    break;
            }
        }


    } while(false);

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

    (*params) = params_descriptors_;

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
