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

#include "mfx_gralloc1.h"

#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_utils.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_gralloc1"

MfxGralloc1Module::~MfxGralloc1Module()
{
    MFX_DEBUG_TRACE_FUNC;

    if (m_gralloc1_dev) gralloc1_close(m_gralloc1_dev);
}

c2_status_t MfxGralloc1Module::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    int hw_res = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &m_hwModule);
    if (hw_res != 0) res = C2_NOT_FOUND;

    if (res == C2_OK) {
        int32_t gr1_err = GRALLOC1_ERROR_NONE;
        do {
            gr1_err = gralloc1_open(m_hwModule, &m_gralloc1_dev);

            if (GRALLOC1_ERROR_NONE != gr1_err) {
                res = C2_CORRUPTED;
                MFX_DEBUG_TRACE_P(m_gralloc1_dev);
                break;
            }

            bool functions_acquired =
                m_grGetFormatFunc.Acquire(m_gralloc1_dev) &&
                m_grGetDimensionsFunc.Acquire(m_gralloc1_dev) &&
                m_grGetNumFlexPlanesFunc.Acquire(m_gralloc1_dev) &&
                m_grGetByteStrideFunc.Acquire(m_gralloc1_dev);
                m_grAllocateFunc.Acquire(m_gralloc1_dev) &&
                m_grReleaseFunc.Acquire(m_gralloc1_dev) &&
                m_grLockFunc.Acquire(m_gralloc1_dev) &&
                m_grUnlockFunc.Acquire(m_gralloc1_dev) &&
                m_grCreateDescriptorFunc.Acquire(m_gralloc1_dev) &&
                m_grSetConsumerUsageFunc.Acquire(m_gralloc1_dev) &&
                m_grSetProducerUsageFunc.Acquire(m_gralloc1_dev) &&
                m_grSetDimensionsFunc.Acquire(m_gralloc1_dev) &&
                m_grSetFormatFunc.Acquire(m_gralloc1_dev) &&
                m_grDestroyDescriptorFunc.Acquire(m_gralloc1_dev) &&
                m_grImportBufferFunc.Acquire(m_gralloc1_dev) &&
                m_grGetBackingStoreFunc.Acquire(m_gralloc1_dev);
#ifdef MFX_C2_USE_PRIME
            if (m_grGetPrimeFunc.Acquire(m_gralloc1_dev)) {
                MFX_DEBUG_TRACE_MSG("Use PRIME");
            } else {
                MFX_DEBUG_TRACE_MSG("Use GRALLOC");
            }
#else
            MFX_DEBUG_TRACE_MSG("Use GRALLOC");
#endif

            if (!functions_acquired) {
                res = C2_CORRUPTED;
                gralloc1_close(m_gralloc1_dev);
                m_gralloc1_dev = nullptr;
            }
        } while (false);
    }
    return res;
}

c2_status_t MfxGralloc1Module::GetBufferDetails(const buffer_handle_t handle,
                                                MfxGralloc1Module::BufferDetails *details)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    int format {};
    int32_t errGetFormat = (*m_grGetFormatFunc)(m_gralloc1_dev, handle, &(format));

        uint32_t planes_count {};
        int32_t errGetPlanes = (*m_grGetNumFlexPlanesFunc)(m_gralloc1_dev, handle, &(planes_count));

        uint32_t pitches[C2PlanarLayout::MAX_NUM_PLANES] {};
        int32_t errGetByteStride = (*m_grGetByteStrideFunc)(m_gralloc1_dev, handle, pitches, planes_count);

    int32_t prime {-1};
    int32_t errGetPrime = GRALLOC1_ERROR_NONE;
#ifdef MFX_C2_USE_PRIME
    if (!(m_grGetPrimeFunc == nullptr)) {
        errGetPrime = (*m_grGetPrimeFunc)(m_gralloc1_dev, handle, (uint32_t*)(&(prime)));
    }
#endif

    uint32_t width {};
    uint32_t height {};
    int32_t errGetDimensions = (*m_grGetDimensionsFunc)(m_gralloc1_dev, handle, &width, &height);

    if (GRALLOC1_ERROR_NONE == errGetFormat &&
        GRALLOC1_ERROR_NONE == errGetPlanes &&
        GRALLOC1_ERROR_NONE == errGetByteStride &&
        GRALLOC1_ERROR_NONE == errGetDimensions &&
        GRALLOC1_ERROR_NONE == errGetPrime)
    {
        details->handle = handle;
        details->prime = prime;
        details->width = details->allocWidth = width;
        details->height = details->allocHeight = height;
        details->format = format;
        details->planes_count = planes_count;
        std::copy(pitches, pitches + C2PlanarLayout::MAX_NUM_PLANES, details->pitches);
    }
    else
    {
        res = C2_CORRUPTED;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxGralloc1Module::Alloc(const uint16_t width, const uint16_t height, buffer_handle_t* handle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    MFX_DEBUG_TRACE_I32(width);
    MFX_DEBUG_TRACE_I32(height);

    int32_t gr1_err = GRALLOC1_ERROR_NONE;
    gralloc1_buffer_descriptor_t descriptor = 0;

    do {
        gr1_err = (*m_grCreateDescriptorFunc)(m_gralloc1_dev, &descriptor);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*m_grSetConsumerUsageFunc)(m_gralloc1_dev, descriptor, GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*m_grSetProducerUsageFunc)(m_gralloc1_dev, descriptor, GRALLOC1_PRODUCER_USAGE_CPU_WRITE);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*m_grSetDimensionsFunc)(m_gralloc1_dev, descriptor, width, height);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*m_grSetFormatFunc)(m_gralloc1_dev, descriptor, HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*m_grAllocateFunc)(m_gralloc1_dev, 1, &descriptor, handle);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

    } while(false);

    if (0 != descriptor) {
        (*m_grDestroyDescriptorFunc)(m_gralloc1_dev, descriptor);
    }

    if (GRALLOC1_ERROR_NONE != gr1_err)
    {
        MFX_DEBUG_TRACE_I32(gr1_err);
        res = C2_NO_MEMORY;
    }

    MFX_DEBUG_TRACE_P(*handle);
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxGralloc1Module::FreeBuffer(const buffer_handle_t handle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    MFX_DEBUG_TRACE_P(handle);

    if (handle) {
        int32_t gr1_err = (*m_grReleaseFunc)(m_gralloc1_dev, handle);
        if (GRALLOC1_ERROR_NONE != gr1_err)
        {
            MFX_DEBUG_TRACE_I32(gr1_err);
            res = C2_BAD_VALUE;
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxGralloc1Module::LockFrame(buffer_handle_t handle, uint8_t** data, C2PlanarLayout *layout)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_P(handle);

    c2_status_t res = C2_OK;


    if (!layout) res = C2_BAD_VALUE;

    BufferDetails details;
    if (C2_OK == res) {
        res = GetBufferDetails(handle, &details);
    }

    mfxU8 *img = NULL;
    if (C2_OK == res) {
        gralloc1_rect_t rect;
        rect.left   = 0;
        rect.top    = 0;
        rect.width  = details.width;
        rect.height = details.height;

        int32_t err = (*m_grLockFunc)(m_gralloc1_dev,
                                   (buffer_handle_t)handle,
                                   GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK,
                                   GRALLOC_USAGE_SW_READ_MASK | GRALLOC_USAGE_SW_WRITE_MASK,
                                   &rect,
                                   (void**)&img,
                                   -1);

        if (GRALLOC1_ERROR_NONE != err || !img)
        {
            res = C2_BAD_STATE;
        }
    }

    if (C2_OK == res) {
        InitNV12PlaneLayout(details.pitches, layout);
        InitNV12PlaneData(details.pitches[C2PlanarLayout::PLANE_Y], details.allocHeight, img, data);
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxGralloc1Module::UnlockFrame(buffer_handle_t handle)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_P(handle);
    c2_status_t res = C2_OK;

    int32_t releaseFence = -1;
    int32_t gr1_res = (*m_grUnlockFunc)(m_gralloc1_dev, (buffer_handle_t)handle, &releaseFence);
    if (GRALLOC1_ERROR_NONE != gr1_res)
    {
        MFX_DEBUG_TRACE_I32(gr1_res);
        res = C2_BAD_STATE;
    }
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

buffer_handle_t MfxGralloc1Module::ImportBuffer(const buffer_handle_t rawHandle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;
    buffer_handle_t outBuffer = nullptr;

    int32_t gr1_res = (*m_grImportBufferFunc)(m_gralloc1_dev, rawHandle, &outBuffer);
    if (GRALLOC1_ERROR_NONE != gr1_res) {
        MFX_DEBUG_TRACE_I32(gr1_res);
        res = C2_BAD_STATE;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return outBuffer;
}

c2_status_t MfxGralloc1Module::GetBackingStore(const buffer_handle_t rawHandle, uint64_t *id)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;
    int32_t gr1_res = (*m_grGetBackingStoreFunc)(m_gralloc1_dev, rawHandle, id);
    if (GRALLOC1_ERROR_NONE != gr1_res) {
        MFX_DEBUG_TRACE_I32(gr1_res);
        res = C2_BAD_STATE;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}
