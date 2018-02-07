/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <mfx_defs.h>
#include <utils/Errors.h>
#include <C2Buffer.h>

#ifdef MFX_C2_USE_GRALLOC_1
    #include <hardware/gralloc1.h>
#else
    #include <hardware/gralloc.h>
#endif

class MfxGrallocModule
{
public:
    static android::c2_status_t Create(std::unique_ptr<MfxGrallocModule>* module);

    virtual ~MfxGrallocModule();

public:
    struct BufferDetails
    {
        int width;
        int height;
        int format;
        uint32_t pitch;
        uint32_t allocWidth;
        uint32_t allocHeight;
    };

public:
    android::c2_status_t GetBufferDetails(const buffer_handle_t handle, BufferDetails* details);

protected:
    MfxGrallocModule() = default;

    android::c2_status_t Init();

protected:
    hw_module_t const* hw_module_ {};
#ifdef MFX_C2_USE_GRALLOC_1

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
    };

    gralloc1_device_t* gralloc1_dev_ {};

    Gralloc1Func<GRALLOC1_PFN_GET_FORMAT, GRALLOC1_FUNCTION_GET_FORMAT> gr_get_format_;
    Gralloc1Func<GRALLOC1_PFN_GET_DIMENSIONS, GRALLOC1_FUNCTION_GET_DIMENSIONS> gr_get_dimensions_;
    Gralloc1Func<GRALLOC1_PFN_GET_STRIDE, GRALLOC1_FUNCTION_GET_STRIDE> gr_get_stride_;
#else
    gralloc_module_t* gralloc_module_ {};
#endif
};

class MfxGrallocAllocator : public MfxGrallocModule
{
public:
    static android::c2_status_t Create(std::unique_ptr<MfxGrallocAllocator>* allocator);

    virtual ~MfxGrallocAllocator();

    virtual android::c2_status_t Alloc(const uint16_t width, const uint16_t height, buffer_handle_t* handle);
    virtual android::c2_status_t Free(const buffer_handle_t handle);
    virtual android::c2_status_t LockFrame(buffer_handle_t handle, uint8_t** data, android::C2PlanarLayout *layout);
    virtual android::c2_status_t UnlockFrame(buffer_handle_t handle);

private:
    MfxGrallocAllocator() = default;

    android::c2_status_t Init();

protected:

#ifdef MFX_C2_USE_GRALLOC_1

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

#else
    alloc_device_t* alloc_dev_ {};
#endif

    MFX_CLASS_NO_COPY(MfxGrallocAllocator)
};
