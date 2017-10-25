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

#include <ufo/gralloc.h>

class MfxGrallocAllocator
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
    hw_module_t const* m_module {};

    alloc_device_t*   m_allocDev {};
    gralloc_module_t* m_grallocModule {}; // for lock()/unlock()

    MFX_CLASS_NO_COPY(MfxGrallocAllocator)
};
