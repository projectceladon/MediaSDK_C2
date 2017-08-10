/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_param_reflector.h"
#include "mfx_debug.h"
#include "mfx_c2_utils.h"

using namespace android;

bool MfxC2ParamReflector::ValidateParam(const C2Param* param,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    bool res = true;

    do {

        C2Param::BaseIndex base_index = C2Param::Type(param->type()).paramIndex();
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
                    C2ParamField param_field(param,
                        (uint32_t*)(uintptr_t)(field_desc.offset() + sizeof(C2Param)));

                    auto it_supported = params_supported_values_.find(param_field);
                    if(it_supported != params_supported_values_.end()) {
                        const C2FieldSupportedValues& supported = it_supported->second;
                        switch(supported.type) {
                            case C2FieldSupportedValues::RANGE: {
                                uint32_t min = supported.range.min.u32;
                                uint32_t max = supported.range.max.u32;
                                const uint8_t* ptr_val = (const uint8_t*)param
                                    + sizeof(C2Param) + field_desc.offset();
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

std::unique_ptr<C2SettingResult> MfxC2ParamReflector::FindParam(const C2Param* param)
{
    return FindC2Param(params_descriptors_, param);
}

status_t MfxC2ParamReflector::getSupportedParams(
    std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const
{
    MFX_DEBUG_TRACE_FUNC;

    (*params) = params_descriptors_;

    return C2_OK;
}
