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
