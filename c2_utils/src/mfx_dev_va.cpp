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

#ifdef LIBVA_SUPPORT

#include "mfx_dev_va.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#include <va/va_android.h>
#include <va/va_backend.h>

#include <sys/ioctl.h>

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_dev_va"

MfxDevVa::MfxDevVa(Usage usage):
    m_usage(usage)
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

    if (!m_bVaInitialized) {

        m_vaDisplay = vaGetDisplay(&m_displayId);

        MFX_DEBUG_TRACE_STREAM(NAMED(m_vaDisplay));

        int major_version = 0, minor_version = 0;
        VAStatus va_res = vaInitialize(m_vaDisplay, &major_version, &minor_version);
        if (VA_STATUS_SUCCESS == va_res) {
            MFX_LOG_INFO("Driver version is %s", vaQueryVendorString(m_vaDisplay));
            m_bVaInitialized = true;
        }
        else
        {
            MFX_LOG_ERROR("vaInitialize failed with an error 0x%X", va_res);
            mfx_res = MFX_ERR_UNKNOWN;
        }
    }

    if (MFX_ERR_NONE == mfx_res) {
        switch(m_usage) {
            case Usage::Encoder:
                if (!m_vaAllocator) {
                    m_vaAllocator.reset(new (std::nothrow)MfxVaFrameAllocator(m_vaDisplay));
                    if (nullptr == m_vaAllocator) mfx_res = MFX_ERR_MEMORY_ALLOC;
                }
                break;
            case Usage::Decoder:
                if (!m_vaPoolAllocator) {
                    m_vaPoolAllocator.reset(new (std::nothrow)MfxVaFramePoolAllocator(m_vaDisplay));
                    if (nullptr == m_vaPoolAllocator) mfx_res = MFX_ERR_MEMORY_ALLOC;
                }
                break;
            default:
                mfx_res = MFX_ERR_UNKNOWN;
                break;
        }
    }


    if (MFX_ERR_NONE != mfx_res) Close();

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxDevVa::Close()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;
    // Define weak_ptrs to allocators to check if they exist.
    std::weak_ptr<MfxVaFrameAllocator> weak_va_allocator { m_vaAllocator };
    std::weak_ptr<MfxVaFramePoolAllocator> weak_va_pool_allocator { m_vaPoolAllocator };

    m_vaAllocator.reset();
    m_vaPoolAllocator.reset();

    // If an allocator exists then some error in resource release order.
    if (!weak_va_allocator.expired() || !weak_va_pool_allocator.expired()) {
        MFX_DEBUG_TRACE_MSG("MfxDevVa allocator is still in use while device is closed");
        res = MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (m_bVaInitialized) {
        MFX_DEBUG_TRACE_STREAM(NAMED(m_vaDisplay));
        vaTerminate(m_vaDisplay);
        m_bVaInitialized = false;
    }

    return res;
}

#ifdef USE_ONEVPL
mfxStatus MfxDevVa::InitMfxSession(mfxSession session)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(session);

    if(session != nullptr) {
        mfx_res =  MFXVideoCORE_SetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL)m_vaDisplay);
        MFX_DEBUG_TRACE_MSG("SetHandle result:");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);

        if(mfx_res == MFX_ERR_UNDEFINED_BEHAVIOR) {
            MFX_DEBUG_TRACE_MSG("Check if the same handle is already set");
            VADisplay dpy = NULL;
            mfxStatus sts = MFXVideoCORE_GetHandle(session, static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL*)&dpy);
            if(sts == MFX_ERR_NONE) {
                if(dpy == m_vaDisplay) {
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
#else
mfxStatus MfxDevVa::InitMfxSession(MFXVideoSession* session)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(session);

    if(session != nullptr) {
        mfx_res = session->SetHandle(
            static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL)m_vaDisplay);
        MFX_DEBUG_TRACE_MSG("SetHandle result:");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);

        if(mfx_res == MFX_ERR_UNDEFINED_BEHAVIOR) {
            MFX_DEBUG_TRACE_MSG("Check if the same handle is already set");
            VADisplay dpy = NULL;
            mfxStatus sts = session->GetHandle(static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL*)&dpy);
            if(sts == MFX_ERR_NONE) {
                if(dpy == m_vaDisplay) {
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
#endif

std::shared_ptr<MfxFrameAllocator> MfxDevVa::GetFrameAllocator()
{
    MFX_DEBUG_TRACE_FUNC;
    return m_usage == Usage::Decoder ? m_vaPoolAllocator : m_vaAllocator;
}

std::shared_ptr<MfxFrameConverter> MfxDevVa::GetFrameConverter()
{
    MFX_DEBUG_TRACE_FUNC;
    return m_usage == Usage::Decoder ? m_vaPoolAllocator : m_vaAllocator;
}

std::shared_ptr<MfxFramePoolAllocator> MfxDevVa::GetFramePoolAllocator()
{
    MFX_DEBUG_TRACE_FUNC;
    return m_usage == Usage::Decoder ? m_vaPoolAllocator : nullptr;
}

#endif // #ifdef LIBVA_SUPPORT
