/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_dev.h"
#include "mfx_defs.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#include "mfx_dev_android.h"

#ifdef LIBVA_SUPPORT
    #include "mfx_dev_va.h"
#endif // #ifdef LIBVA_SUPPORT

mfxStatus MfxDev::Create(std::unique_ptr<MfxDev>* device)
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<MfxDev> created_dev;

    mfxStatus sts = MFX_ERR_NONE;

#ifdef LIBVA_SUPPORT
    created_dev.reset(MFX_NEW_NO_THROW(MfxDevVa()));
#else
    created_dev.reset(MFX_NEW_NO_THROW(MfxDevAndroid()));
#endif
    if(created_dev == nullptr) {
        sts = MFX_ERR_MEMORY_ALLOC;
    }

    if(created_dev != nullptr) {
        sts = created_dev->Init();
        if(sts == MFX_ERR_NONE) {
            *device = std::move(created_dev);
        }
    }

    MFX_DEBUG_TRACE_P(device->get());
    MFX_DEBUG_TRACE_mfxStatus(sts);

    return sts;
}
