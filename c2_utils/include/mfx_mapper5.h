// Copyright (c) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <android/hardware/graphics/mapper/IMapper.h>
#include <mfx_defs.h>
#include <utils/Errors.h>
#include <hidl/HidlSupport.h>
#include "mfx_gralloc_interface.h"

#ifdef USE_MAPPER5
using namespace android;
using hardware::hidl_vec;
using hardware::hidl_handle;

class MfxMapper5Module : public IMfxGrallocModule
{
public:
    virtual c2_status_t Init() override;

    virtual ~MfxMapper5Module();

    virtual c2_status_t GetBufferDetails(const buffer_handle_t handle,
                                         BufferDetails* details) override;
    virtual c2_status_t GetBackingStore(const buffer_handle_t rawHandle, uint64_t *id) override;
    virtual buffer_handle_t ImportBuffer(const buffer_handle_t rawHandle) override;
    virtual c2_status_t FreeBuffer(const buffer_handle_t rawHandle);
    virtual c2_status_t LockFrame(buffer_handle_t handle, uint8_t** data, C2PlanarLayout *layout);
    virtual c2_status_t UnlockFrame(buffer_handle_t handle);

private:
    AIMapper *m_mapper;
};

#endif
