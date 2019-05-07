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

std::unique_ptr<C2StructDescriptor> MfxC2ParamReflector::describe(
    C2Param::CoreIndex coreIndex) const
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(descriptors_mutex_);

    MFX_DEBUG_TRACE_STREAM(std::hex << NAMED(coreIndex.coreIndex()));

    std::unique_ptr<C2StructDescriptor> result;

    auto found_struct = params_struct_descriptors_.find(C2Param::Type(coreIndex.coreIndex()));
    if(found_struct != params_struct_descriptors_.end()) {
        result = std::make_unique<C2StructDescriptor>(found_struct->second);
    }

    return result;
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

void MfxC2ParamReflector::DumpParams()
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(descriptors_mutex_);

    const std::string indent(4, ' ');

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
}

#endif // MFX_DEBUG == MFX_DEBUG_YES
