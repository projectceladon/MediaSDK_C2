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

#include "mfx_gralloc4.h"
#include "cros_gralloc/cros_gralloc_helpers.h"
#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"

#ifdef USE_GRALLOC4

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_gralloc4"

using aidl::android::hardware::graphics::common::PlaneLayout;

c2_status_t MfxGralloc4Module::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    sp<IMapper4> mapper = IMapper4::getService();
    if (nullptr == mapper)
    {
        return C2_CORRUPTED;
    }

    m_mapper = std::move(mapper);
    
    return C2_OK;
}

MfxGralloc4Module::~MfxGralloc4Module()
{

}

Error4 MfxGralloc4Module::Get(const native_handle_t* handle, const IMapper4::MetadataType& metadataType,
                    hidl_vec<uint8_t>& outVec)
{
    Error4 err;
    if (nullptr == m_mapper)
        return Error4::NO_RESOURCES;
    m_mapper->get(const_cast<native_handle_t*>(handle), metadataType,
                [&](const auto& tmpError, const hidl_vec<uint8_t>& tmpVec)
                {
                    err = tmpError;
                    outVec = tmpVec;
                });
    return err;
}

Error4 MfxGralloc4Module::GetWithImported(const native_handle_t* handle, const IMapper4::MetadataType& metadataType,
                    hidl_vec<uint8_t>& outVec)
{
    Error4 err;
    if (nullptr == m_mapper)
        return Error4::NO_RESOURCES;

    auto importedHnd = ImportBuffer(handle);
    m_mapper->get(const_cast<native_handle_t*>(importedHnd), metadataType,
                [&](const auto& tmpError, const hidl_vec<uint8_t>& tmpVec)
                {
                    err = tmpError;
                    outVec = tmpVec;
                });

    (void)FreeBuffer(importedHnd);
    return err;
}

c2_status_t MfxGralloc4Module::GetBufferDetails(const buffer_handle_t handle, BufferDetails *details)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    buffer_handle_t importedHnd = nullptr;
    do
    {
        importedHnd = ImportBuffer(handle);
        details->handle = handle;

        details->prime = handle->data[0];
        MFX_DEBUG_TRACE_I32(details->prime);

        hidl_vec<uint8_t> vec;

        if (IsFailed(Get(importedHnd, gralloc4::MetadataType_Width, vec)))
        {
            res = C2_CORRUPTED;
            break;
        }

        uint64_t width = 0;
        gralloc4::decodeWidth(vec, &width);
        details->width = details->allocWidth = width;
        MFX_DEBUG_TRACE_I32(details->width);

        if (IsFailed(Get(importedHnd, gralloc4::MetadataType_Height, vec)))
        {
            res = C2_CORRUPTED;
            break;
        }

        uint64_t height = 0;
        gralloc4::decodeHeight(vec, &height);
        details->height = details->allocHeight = height;
        MFX_DEBUG_TRACE_I32(details->height);

        hardware::graphics::common::V1_2::PixelFormat pixelFormat;
        if (IsFailed(Get(importedHnd, gralloc4::MetadataType_PixelFormatRequested, vec)))
        {
            res = C2_CORRUPTED;
            break;
        }
        gralloc4::decodePixelFormatRequested(vec, &pixelFormat);
        details->format = static_cast<int>(pixelFormat);
        MFX_DEBUG_TRACE_I32(details->format);

        if(IsFailed(Get(importedHnd, gralloc4::MetadataType_PlaneLayouts, vec)))
        {
            res = C2_CORRUPTED;
            break;
        }

        std::vector<PlaneLayout> planeLayouts;
        if (NO_ERROR != gralloc4::decodePlaneLayouts(vec, &planeLayouts))
        {
            res = C2_CORRUPTED;
            break;
        }

        details->planes_count = planeLayouts.size();
        MFX_DEBUG_TRACE_I32(details->planes_count);

        for(int i = 0; i < planeLayouts.size(); i++)
        {
            details->pitches[i] = planeLayouts[i].strideInBytes;
            MFX_DEBUG_TRACE_STREAM("details->pitches[" << i << "] = " << details->pitches[i]);
        }
    } while (false);

    (void)FreeBuffer(importedHnd);
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxGralloc4Module::GetBackingStore(const buffer_handle_t handle, uint64_t *id)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    hidl_vec<uint8_t> vec;
    if(IsFailed(GetWithImported(handle, android::gralloc4::MetadataType_BufferId, vec)))
        res = C2_CORRUPTED;
    gralloc4::decodeBufferId(vec, id);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

buffer_handle_t MfxGralloc4Module::ImportBuffer(const buffer_handle_t rawHandle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;
    buffer_handle_t outBuffer = nullptr;
    Error4 err;

    if (nullptr == m_mapper)
        res = C2_CORRUPTED;
    if (C2_OK == res)
    {
        m_mapper->importBuffer(hardware::hidl_handle(rawHandle), [&](const Error4 & tmpError, void * tmpBuffer) {
            err = tmpError;
            outBuffer = static_cast<buffer_handle_t>(tmpBuffer);
        });
        if (IsFailed(err))
            res = C2_CORRUPTED;
    }
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return outBuffer;
}

c2_status_t MfxGralloc4Module::FreeBuffer(const buffer_handle_t rawHandle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;
    Error4 err;
    if (nullptr == m_mapper)
        res = C2_CORRUPTED;
    if (C2_OK == res)
    {
        err = m_mapper->freeBuffer(const_cast<native_handle_t*>(rawHandle));

        if (IsFailed(err))
            res = C2_CORRUPTED;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxGralloc4Module::LockFrame(buffer_handle_t handle, uint8_t** data, C2PlanarLayout *layout)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    if (!layout)
        return C2_BAD_VALUE;

    if (nullptr == m_mapper)
        return C2_NO_INIT;

    BufferDetails details;

    if (C2_OK != GetBufferDetails(handle, &details))
        return C2_BAD_VALUE;

    native_handle_t *native_handle = const_cast<native_handle_t *>(handle);

    IMapper4::Rect rect;
    rect.left = 0;
    rect.top = 0;
    rect.width = details.width;
    rect.height = details.height;

    hidl_handle empty_fence_handle;

    Error4 error;
    void **img = nullptr;
    m_mapper->lock(native_handle,
                    AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_READ_MASK |
                    AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK,
                    rect, empty_fence_handle,
                    [&](const auto &tmp_err, const auto &tmp_vaddr) {
                        error = tmp_err;
                        if(tmp_err == Error4::NONE)
                        {
                            *img = tmp_vaddr;
                        }
                    });
    res = error == Error4::NONE ? C2_OK : C2_CORRUPTED;

    if (C2_OK == res) {
        InitNV12PlaneLayout(details.pitches, layout);
        InitNV12PlaneData(details.pitches[C2PlanarLayout::PLANE_Y], details.allocHeight, (uint8_t*)*img, data);
    }

    return res;
}

c2_status_t MfxGralloc4Module::UnlockFrame(buffer_handle_t handle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    if (nullptr == m_mapper)
        return C2_NO_INIT;

    native_handle_t *native_handle = const_cast<native_handle_t *>(handle);

    Error4 error;
    m_mapper->unlock(native_handle,
                    [&](const auto &tmp_err, const auto &) {
                        error = tmp_err;
                    });
    res = error == Error4::NONE ? C2_OK : C2_CORRUPTED;

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

#endif
