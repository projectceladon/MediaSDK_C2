/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <mfx_defs.h>
#include <utils/Errors.h>
#include <C2Buffer.h>
#include <hardware/gralloc1.h>

class MfxGrallocModule
{
public:
    static c2_status_t Create(std::unique_ptr<MfxGrallocModule>* module);

    virtual ~MfxGrallocModule();

public:
    struct BufferDetails
    {
        buffer_handle_t handle;
        int32_t prime;
        int width;
        int height;
        int format;
        uint32_t pitch;
        uint32_t allocWidth;
        uint32_t allocHeight;
        BufferDetails():
            handle(nullptr),
            prime(-1),
            width(0),
            height(0),
            format(0),
            pitch(0),
            allocWidth(0),
            allocHeight(0)
        {}
    };

public:
    c2_status_t GetBufferDetails(const buffer_handle_t handle, BufferDetails* details);

protected:
    MfxGrallocModule() = default;

    c2_status_t Init();

protected:
    hw_module_t const* hw_module_ {};

    template<typename FuncType, gralloc1_function_descriptor_t FuncId>
    class Gralloc1Func
    {
    private:
        FuncType func_ {};
    public:
        FuncType operator*() { return func_; }
        bool Acquire(gralloc1_device_t* gr_device)
        {
            func_ = (FuncType)gr_device->getFunction(gr_device, FuncId);
            return func_ != nullptr;
        }
        bool operator==(FuncType const &right) { return func_ == right; }
    };

    gralloc1_device_t* gralloc1_dev_ {};

    Gralloc1Func<GRALLOC1_PFN_GET_FORMAT, GRALLOC1_FUNCTION_GET_FORMAT> gr_get_format_;
    Gralloc1Func<GRALLOC1_PFN_GET_DIMENSIONS, GRALLOC1_FUNCTION_GET_DIMENSIONS> gr_get_dimensions_;
    Gralloc1Func<GRALLOC1_PFN_GET_STRIDE, GRALLOC1_FUNCTION_GET_STRIDE> gr_get_stride_;
#ifdef MFX_C2_USE_PRIME
    Gralloc1Func<GRALLOC1_PFN_GET_PRIME, (gralloc1_function_descriptor_t)GRALLOC1_FUNCTION_GET_PRIME> gr_get_prime_;
#endif
};

class MfxGrallocAllocator : public MfxGrallocModule
{
public:
    static c2_status_t Create(std::unique_ptr<MfxGrallocAllocator>* allocator);

    virtual c2_status_t Alloc(const uint16_t width, const uint16_t height, buffer_handle_t* handle);
    virtual c2_status_t Free(const buffer_handle_t handle);
    virtual c2_status_t LockFrame(buffer_handle_t handle, uint8_t** data, C2PlanarLayout *layout);
    virtual c2_status_t UnlockFrame(buffer_handle_t handle);

private:
    MfxGrallocAllocator() = default;

    c2_status_t Init();

protected:
    Gralloc1Func<GRALLOC1_PFN_ALLOCATE, GRALLOC1_FUNCTION_ALLOCATE> gr_allocate_;
    Gralloc1Func<GRALLOC1_PFN_RELEASE, GRALLOC1_FUNCTION_RELEASE> gr_release_;
    Gralloc1Func<GRALLOC1_PFN_LOCK, GRALLOC1_FUNCTION_LOCK> gr_lock_;
    Gralloc1Func<GRALLOC1_PFN_UNLOCK, GRALLOC1_FUNCTION_UNLOCK> gr_unlock_;
    Gralloc1Func<GRALLOC1_PFN_CREATE_DESCRIPTOR, GRALLOC1_FUNCTION_CREATE_DESCRIPTOR> gr_create_descriptor_;
    Gralloc1Func<GRALLOC1_PFN_SET_CONSUMER_USAGE, GRALLOC1_FUNCTION_SET_CONSUMER_USAGE> gr_set_consumer_usage_;
    Gralloc1Func<GRALLOC1_PFN_SET_PRODUCER_USAGE, GRALLOC1_FUNCTION_SET_PRODUCER_USAGE> gr_set_producer_usage_;
    Gralloc1Func<GRALLOC1_PFN_SET_DIMENSIONS, GRALLOC1_FUNCTION_SET_DIMENSIONS> gr_set_dimensions_;
    Gralloc1Func<GRALLOC1_PFN_SET_FORMAT, GRALLOC1_FUNCTION_SET_FORMAT> gr_set_format_;
    Gralloc1Func<GRALLOC1_PFN_DESTROY_DESCRIPTOR, GRALLOC1_FUNCTION_DESTROY_DESCRIPTOR> gr_destroy_descriptor_;

    MFX_CLASS_NO_COPY(MfxGrallocAllocator)
};
