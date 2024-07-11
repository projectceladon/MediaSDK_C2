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

#include <mfx_defs.h>
#include <utils/Errors.h>
#include <hardware/gralloc1.h>
#include "mfx_gralloc_interface.h"

class MfxGralloc1Module : public IMfxGrallocModule
{
MFX_CLASS_NO_COPY(MfxGralloc1Module)
public:
    MfxGralloc1Module() = default;
    virtual ~MfxGralloc1Module();

    virtual c2_status_t Init() override;

    virtual c2_status_t GetBackingStore(const buffer_handle_t rawHandle, uint64_t *id) override;
    virtual c2_status_t GetBufferDetails(const buffer_handle_t handle, BufferDetails* details) override;

    virtual c2_status_t Alloc(const uint16_t width, const uint16_t height, buffer_handle_t* handle);
    virtual c2_status_t Free(const buffer_handle_t handle);
    virtual c2_status_t LockFrame(buffer_handle_t handle, uint8_t** data, C2PlanarLayout *layout);
    virtual c2_status_t UnlockFrame(buffer_handle_t handle);
    virtual buffer_handle_t ImportBuffer(const buffer_handle_t rawHandle) override;

protected:
    hw_module_t const* m_hwModule {};

    template<typename FuncType, gralloc1_function_descriptor_t FuncId>
    class Gralloc1Func
    {
    private:
        FuncType m_func {};
    public:
        FuncType operator*() { return m_func; }
        bool Acquire(gralloc1_device_t* gr_device)
        {
            m_func = (FuncType)gr_device->getFunction(gr_device, FuncId);
            return m_func != nullptr;
        }
        bool operator==(FuncType const &right) { return m_func == right; }
    };

    gralloc1_device_t* m_gralloc1_dev {};

    Gralloc1Func<GRALLOC1_PFN_GET_FORMAT, GRALLOC1_FUNCTION_GET_FORMAT> m_grGetFormatFunc;
    Gralloc1Func<GRALLOC1_PFN_GET_DIMENSIONS, GRALLOC1_FUNCTION_GET_DIMENSIONS> m_grGetDimensionsFunc;
    Gralloc1Func<GRALLOC1_PFN_GET_NUM_FLEX_PLANES, GRALLOC1_FUNCTION_GET_NUM_FLEX_PLANES> m_grGetNumFlexPlanesFunc;
    Gralloc1Func<GRALLOC1_PFN_GET_BYTE_STRIDE, (gralloc1_function_descriptor_t)GRALLOC1_FUNCTION_GET_BYTE_STRIDE> m_grGetByteStrideFunc;
    Gralloc1Func<GRALLOC1_PFN_ALLOCATE, GRALLOC1_FUNCTION_ALLOCATE> m_grAllocateFunc;
    Gralloc1Func<GRALLOC1_PFN_RELEASE, GRALLOC1_FUNCTION_RELEASE> m_grReleaseFunc;
    Gralloc1Func<GRALLOC1_PFN_LOCK, GRALLOC1_FUNCTION_LOCK> m_grLockFunc;
    Gralloc1Func<GRALLOC1_PFN_UNLOCK, GRALLOC1_FUNCTION_UNLOCK> m_grUnlockFunc;
    Gralloc1Func<GRALLOC1_PFN_CREATE_DESCRIPTOR, GRALLOC1_FUNCTION_CREATE_DESCRIPTOR> m_grCreateDescriptorFunc;
    Gralloc1Func<GRALLOC1_PFN_SET_CONSUMER_USAGE, GRALLOC1_FUNCTION_SET_CONSUMER_USAGE> m_grSetConsumerUsageFunc;
    Gralloc1Func<GRALLOC1_PFN_SET_PRODUCER_USAGE, GRALLOC1_FUNCTION_SET_PRODUCER_USAGE> m_grSetProducerUsageFunc;
    Gralloc1Func<GRALLOC1_PFN_SET_DIMENSIONS, GRALLOC1_FUNCTION_SET_DIMENSIONS> m_grSetDimensionsFunc;
    Gralloc1Func<GRALLOC1_PFN_SET_FORMAT, GRALLOC1_FUNCTION_SET_FORMAT> m_grSetFormatFunc;
    Gralloc1Func<GRALLOC1_PFN_DESTROY_DESCRIPTOR, GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR> m_grDestroyDescriptorFunc;
    Gralloc1Func<GRALLOC1_PFN_IMPORT_BUFFER, GRALLOC1_FUNCTION_IMPORT_BUFFER> m_grImportBufferFunc;
    Gralloc1Func<GRALLOC1_PFN_GET_BACKING_STORE, GRALLOC1_FUNCTION_GET_BACKING_STORE> m_grGetBackingStoreFunc;
#ifdef MFX_C2_USE_PRIME
    Gralloc1Func<GRALLOC1_PFN_GET_PRIME, (gralloc1_function_descriptor_t)GRALLOC1_FUNCTION_GET_PRIME> m_grGetPrimeFunc;
#endif
};
