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

#include <hardware/gralloc.h>

class MfxGrallocModule
{
public:
    static android::status_t Create(std::unique_ptr<MfxGrallocModule>* module);

    virtual ~MfxGrallocModule() = default;

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
    android::status_t GetBufferDetails(const buffer_handle_t handle, BufferDetails* details);

protected:
    MfxGrallocModule() = default;

    android::status_t Init();

protected:
    hw_module_t const* m_module {};

    gralloc_module_t* m_grallocModule {};
};

class MfxGrallocAllocator : public MfxGrallocModule
{
public:
    static android::status_t Create(std::unique_ptr<MfxGrallocAllocator>* allocator);

    virtual ~MfxGrallocAllocator();

    virtual android::status_t Alloc(const uint16_t width, const uint16_t height, buffer_handle_t* handle);
    virtual android::status_t Free(const buffer_handle_t handle);
    virtual android::status_t LockFrame(buffer_handle_t handle, uint8_t** data, android::C2PlaneLayout *layout);
    virtual android::status_t UnlockFrame(buffer_handle_t handle);

private:
    MfxGrallocAllocator() = default;

    android::status_t Init();

protected:

    alloc_device_t*   m_allocDev {};

    MFX_CLASS_NO_COPY(MfxGrallocAllocator)
};
