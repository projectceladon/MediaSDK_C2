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

#include <stdio.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <thread>
#include "mfx_debug.h"
#include <C2Param.h>
#include <C2Component.h>
#include <C2Work.h>
#include <map>

class MfxC2ParamReflector : public C2ParamReflector
{
public:
    template<typename ParamType>
    void AddDescription();

public: // C2ParamReflector
    std::unique_ptr<C2StructDescriptor> describe(
        C2Param::CoreIndex coreIndex) const override;

#if MFX_DEBUG == MFX_DEBUG_YES
    void DumpParams();
#else
    void DumpParams() { }
#endif

private:
    std::map<C2Param::CoreIndex, C2StructDescriptor> params_struct_descriptors_;
    mutable std::mutex descriptors_mutex_;
};

template<typename ParamType>
void MfxC2ParamReflector::AddDescription()
{
    std::lock_guard<std::mutex> lock(descriptors_mutex_);
    params_struct_descriptors_.insert({ C2Param::Type(ParamType::PARAM_TYPE).coreIndex(), C2StructDescriptor(ParamType::PARAM_TYPE, ParamType::FieldList()) });
}

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
