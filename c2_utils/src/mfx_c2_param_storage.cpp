/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_param_storage.h"

#include "mfx_debug.h"
#include "mfx_c2_utils.h"
#include <sstream>

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

c2_status_t MfxC2ParamStorage::QueryParam(C2Param::Index index, C2Param** dst) const
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_STREAM("Param: " << std::hex << (uint32_t)index);

    c2_status_t res = C2_OK;

    auto const_value_found = const_values_.find(index);
    if (const_value_found != const_values_.end()) {
        if (nullptr == *dst) {
            *dst = C2Param::Copy(*const_value_found->second).release();
        }
        else {
            bool copy_res = (*dst)->updateFrom(*const_value_found->second);
            if (!copy_res) {
                (*dst)->invalidate();
                res = C2_NO_MEMORY;
            }
        }

    } else {
        auto operations_found = param_operations_.find(index);
        if (operations_found != param_operations_.end()) {
            const C2ParamOperations& operations = operations_found->second;
            if (nullptr == *dst) {
                *dst = operations.allocate_();
            }
            operations.get_(*dst);
        } else {
            res = C2_NOT_FOUND;
        }
    }
    return res;
}

c2_status_t MfxC2ParamStorage::ConfigParam(const C2Param& param, bool component_stopped,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    do {
        // check whether plugin supports this parameter
        std::unique_ptr<C2SettingResult> find_res = FindParam(&param);
        if(nullptr != find_res) {
            failures->push_back(std::move(find_res));
            break;
        }
        // check whether plugin is in a correct state to apply this parameter
        bool modifiable = ((param.kind() & C2Param::TUNING) != 0) ||
            component_stopped; /* all kinds, even INFO might be set by stagefright */

        if (!modifiable) {
            failures->push_back(MakeC2SettingResult(C2ParamField(&param), C2SettingResult::READ_ONLY));
            break;
        }

        // check ranges
        if(!ValidateParam(&param, failures)) {
            break;
        }

        C2Param::Index index = param.index();
        auto const_value_found = const_values_.find(index);
        if (const_value_found != const_values_.end()) {
            failures->push_back(MakeC2SettingResult(C2ParamField(&param), C2SettingResult::READ_ONLY));
            break;
        }

        auto operations_found = param_operations_.find(index);
        if (operations_found == param_operations_.end()) {
            failures->push_back(MakeC2SettingResult(C2ParamField(&param), C2SettingResult::READ_ONLY));
            break;
        }

        const C2ParamOperations& operations = operations_found->second;
        if (!operations.set_) {
            failures->push_back(MakeC2SettingResult(C2ParamField(&param), C2SettingResult::READ_ONLY));
            break;
        }
        if (!operations.set_(param)) {
            failures->push_back(MakeC2SettingResult(C2ParamField(&param), C2SettingResult::BAD_VALUE));
            break;
        }

    } while(false);

    return GetAggregateStatus(failures);
}

bool MfxC2ParamStorage::ValidateParam(const C2Param* param,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    bool res = true;

    do {

        C2Param::CoreIndex core_index = param->type().coreIndex();

        MFX_DEBUG_TRACE_STREAM(std::hex << NAMED(core_index.coreIndex()));

        std::unique_ptr<C2StructDescriptor> struct_desc = reflector_->describe(core_index);
        if (!struct_desc) {
            MFX_DEBUG_TRACE_MSG("the whole param is not supported");
            res = false;
            failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
            break;
        }

        for(auto it_field = struct_desc->cbegin(); it_field != struct_desc->cend(); ++it_field) {

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

bool MfxC2ParamStorage::FindParam(C2Param::Index param_index) const
{
    return FindC2Param(params_descriptors_, param_index);
}

std::unique_ptr<C2SettingResult> MfxC2ParamStorage::FindParam(const C2Param* param) const
{
    return FindC2Param(params_descriptors_, param);
}

c2_status_t MfxC2ParamStorage::getSupportedParams(
    std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const
{
    MFX_DEBUG_TRACE_FUNC;

    (*params) = params_descriptors_;

    return C2_OK;
}

c2_status_t MfxC2ParamStorage::querySupportedValues_vb(
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

void MfxC2ParamStorage::DumpParams()
{
    MFX_DEBUG_TRACE_FUNC;

    const std::string indent(4, ' ');

    for(auto desc : params_descriptors_) {
        std::ostringstream oss;
        uint32_t index = desc->index();
        oss << std::hex << index << " " << desc->name();
        MFX_DEBUG_TRACE_MSG(oss.str().c_str());
    }

    reflector_->DumpParams();

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