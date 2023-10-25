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
#include "mfx_gralloc_interface.h"

#ifdef USE_GRALLOC4
// #include <android/hardware/graphics/allocator/4.0/IAllocator.h>
#include "gralloctypes/Gralloc4.h"
#include <android/hardware/graphics/mapper/4.0/IMapper.h>

using namespace android;
using hardware::hidl_vec;
using hardware::hidl_handle;
using IMapper4 = ::android::hardware::graphics::mapper::V4_0::IMapper;
using Error4 = ::android::hardware::graphics::mapper::V4_0::Error;
// using IAllocator4 = ::android::hardware::graphics::allocator::V4_0::IAllocator;

class MfxGralloc4Module : public IMfxGrallocModule
{
public:
    virtual c2_status_t Init() override;

    virtual ~MfxGralloc4Module();

    // Wrapper for IMapper::get
    virtual Error4 Get(const native_handle_t* bufferHandle, const IMapper4::MetadataType& metadataType,
                    hidl_vec<uint8_t>& outVec);
    virtual Error4 GetWithImported(const native_handle_t* handle, const IMapper4::MetadataType& metadataType,
                    hidl_vec<uint8_t>& outVec);

    virtual c2_status_t GetBufferDetails(const buffer_handle_t handle, BufferDetails* details) override;
    virtual c2_status_t GetBackingStore(const buffer_handle_t rawHandle, uint64_t *id) override;

    // Start with Android U, the get function of IMapper4 will check whether the buffer handle is reserved.
    // So we need to call importBuffer to preserve handle before getting the buffer's info.
    virtual buffer_handle_t ImportBuffer(const buffer_handle_t rawHandle) override;
    virtual c2_status_t FreeBuffer(const buffer_handle_t rawHandle) override;

    // TODO: not fully tested
    virtual c2_status_t LockFrame(buffer_handle_t handle, uint8_t** data, C2PlanarLayout *layout);
    virtual c2_status_t UnlockFrame(buffer_handle_t handle);

protected:
    inline bool IsFailed(Error4 err) { return (err != Error4::NONE); }

private:
    sp<IMapper4> m_mapper;
};

#endif
