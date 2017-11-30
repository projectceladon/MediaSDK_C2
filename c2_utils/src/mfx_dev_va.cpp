/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifdef LIBVA_SUPPORT

#include "mfx_dev_va.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#include <va/va_android.h>
#include <va/va_backend.h>

#include <sys/ioctl.h>

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_dev_va"

MfxDevVa::MfxDevVa()
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxDevVa::~MfxDevVa()
{
    MFX_DEBUG_TRACE_FUNC;
    Close();
}

mfxStatus MfxDevVa::Init()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    if (!va_initialized_) {

        va_display_ = vaGetDisplay(&display_id_);

        int major_version = 0, minor_version = 0;
        VAStatus va_res = vaInitialize(va_display_, &major_version, &minor_version);
        if (VA_STATUS_SUCCESS == va_res) {
            MFX_LOG_INFO("Driver version is %s", vaQueryVendorString(va_display_));
            va_initialized_ = true;
        }
        else
        {
            MFX_LOG_ERROR("vaInitialize failed with an error 0x%X", va_res);
            mfx_res = MFX_ERR_UNKNOWN;
        }
    }

    if (MFX_ERR_NONE == mfx_res && !va_allocator_) {

        va_allocator_.reset(new (std::nothrow)MfxVaFrameAllocator(va_display_));
        if (nullptr == va_allocator_) mfx_res = MFX_ERR_MEMORY_ALLOC;
    }

    if (MFX_ERR_NONE != mfx_res) Close();

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxDevVa::Close()
{
    MFX_DEBUG_TRACE_FUNC;

    if (nullptr != va_allocator_) va_allocator_ = nullptr;

    if (va_initialized_) {
        vaTerminate(va_display_);
        va_initialized_ = false;
    }

    return MFX_ERR_NONE;
}

mfxStatus MfxDevVa::InitMfxSession(MFXVideoSession* session)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(session);

    if(session != nullptr) {
        mfx_res = session->SetHandle(
            static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL)va_display_);
        MFX_DEBUG_TRACE_MSG("SetHandle result:");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);

        if(mfx_res == MFX_ERR_UNDEFINED_BEHAVIOR) {
            MFX_DEBUG_TRACE_MSG("Check if the same handle is already set");
            VADisplay dpy = NULL;
            mfxStatus sts = session->GetHandle(static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL*)&dpy);
            if(sts == MFX_ERR_NONE) {
                if(dpy == va_display_) {
                    MFX_DEBUG_TRACE_MSG("Same display handle is already set, not an error");
                    mfx_res = MFX_ERR_NONE;
                }
                else {
                    MFX_DEBUG_TRACE_MSG("Different display handle is set");
                }
            } else {
                MFX_DEBUG_TRACE_MSG("GetHandle failed:");
                MFX_DEBUG_TRACE__mfxStatus(sts);
            }
        }
    } else {
        mfx_res = MFX_ERR_NULL_PTR;
    }
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

#endif // #ifdef LIBVA_SUPPORT
