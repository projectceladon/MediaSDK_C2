/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_gralloc_allocator.h"

#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_utils.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_gralloc_allocator"

c2_status_t MfxGrallocModule::Create(std::unique_ptr<MfxGrallocModule>* allocator)
{
    c2_status_t res = C2_OK;
    if (allocator) {
        std::unique_ptr<MfxGrallocModule> alloc(new (std::nothrow)MfxGrallocModule());
        if (alloc) {
            res = alloc->Init();
            if (res == C2_OK) *allocator = std::move(alloc);
        } else {
            res = C2_NO_MEMORY;
        }
    } else {
        res = C2_BAD_VALUE;
    }
    return res;
}

MfxGrallocModule::~MfxGrallocModule()
{
    MFX_DEBUG_TRACE_FUNC;

    if (gralloc1_dev_) gralloc1_close(gralloc1_dev_);
}

c2_status_t MfxGrallocModule::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    int hw_res = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &hw_module_);
    if (hw_res != 0) res = C2_NOT_FOUND;

    if (res == C2_OK) {
        int32_t gr1_err = GRALLOC1_ERROR_NONE;
        do {
            gr1_err = gralloc1_open(hw_module_, &gralloc1_dev_);

            if (GRALLOC1_ERROR_NONE != gr1_err) {
                res = C2_CORRUPTED;
                break;
            }

            bool functions_acquired =
                gr_get_format_.Acquire(gralloc1_dev_) &&
                gr_get_dimensions_.Acquire(gralloc1_dev_) &&
                gr_get_num_flex_planes_.Acquire(gralloc1_dev_) &&
                gr_get_byte_stride_.Acquire(gralloc1_dev_);
#ifdef MFX_C2_USE_PRIME
            if (gr_get_prime_.Acquire(gralloc1_dev_)) {
                MFX_DEBUG_TRACE_MSG("Use PRIME");
            } else {
                MFX_DEBUG_TRACE_MSG("Use GRALLOC");
            }
#else
            MFX_DEBUG_TRACE_MSG("Use GRALLOC");
#endif

            if (!functions_acquired) {
                res = C2_CORRUPTED;
                gralloc1_close(gralloc1_dev_);
                gralloc1_dev_ = nullptr;
            }
        } while (false);
    }
    return res;
}

c2_status_t MfxGrallocModule::GetBufferDetails(const buffer_handle_t handle,
    MfxGrallocModule::BufferDetails* details)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    int format {};
    int32_t errGetFormat = (*gr_get_format_)(gralloc1_dev_, handle, &(format));

        uint32_t planes_count {};
        int32_t errGetPlanes = (*gr_get_num_flex_planes_)(gralloc1_dev_, handle, &(planes_count));

        uint32_t pitches[C2PlanarLayout::MAX_NUM_PLANES] {};
        int32_t errGetByteStride = (*gr_get_byte_stride_)(gralloc1_dev_, handle, pitches, planes_count);

    int32_t prime {-1};
    int32_t errGetPrime = GRALLOC1_ERROR_NONE;
#ifdef MFX_C2_USE_PRIME
    if (!(gr_get_prime_ == nullptr)) {
        errGetPrime = (*gr_get_prime_)(gralloc1_dev_, handle, (uint32_t*)(&(prime)));
    }
#endif

    uint32_t width {};
    uint32_t height {};
    int32_t errGetDimensions = (*gr_get_dimensions_)(gralloc1_dev_, handle, &width, &height);

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
    return res;
}

c2_status_t MfxGrallocAllocator::Create(std::unique_ptr<MfxGrallocAllocator>* allocator)
{
    c2_status_t res = C2_OK;
    if (allocator) {
        std::unique_ptr<MfxGrallocAllocator> alloc(new (std::nothrow)MfxGrallocAllocator());
        if (alloc) {
            res = alloc->Init();
            if (res == C2_OK) *allocator = std::move(alloc);
        } else {
            res = C2_NO_MEMORY;
        }
    } else {
        res = C2_BAD_VALUE;
    }
    return res;
}

c2_status_t MfxGrallocAllocator::Init()
{
    c2_status_t res = MfxGrallocModule::Init();

    if (C2_OK == res) {
        bool functions_acquired =
            gr_allocate_.Acquire(gralloc1_dev_) &&
            gr_release_.Acquire(gralloc1_dev_) &&
            gr_lock_.Acquire(gralloc1_dev_) &&
            gr_unlock_.Acquire(gralloc1_dev_) &&
            gr_create_descriptor_.Acquire(gralloc1_dev_) &&
            gr_set_consumer_usage_.Acquire(gralloc1_dev_) &&
            gr_set_producer_usage_.Acquire(gralloc1_dev_) &&
            gr_set_dimensions_.Acquire(gralloc1_dev_) &&
            gr_set_format_.Acquire(gralloc1_dev_) &&
            gr_destroy_descriptor_.Acquire(gralloc1_dev_);

        if (!functions_acquired) {
            res = C2_CORRUPTED;
            // if MfxGrallocModule::Init allocated some resources
            // its destructor is responsible to free them.
        }
    }
    return res;
}

c2_status_t MfxGrallocAllocator::Alloc(const uint16_t width, const uint16_t height, buffer_handle_t* handle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    MFX_DEBUG_TRACE_I32(width);
    MFX_DEBUG_TRACE_I32(height);

    int32_t gr1_err = GRALLOC1_ERROR_NONE;
    gralloc1_buffer_descriptor_t descriptor = 0;

    do {
        gr1_err = (*gr_create_descriptor_)(gralloc1_dev_, &descriptor);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*gr_set_consumer_usage_)(gralloc1_dev_, descriptor, GRALLOC1_CONSUMER_USAGE_VIDEO_ENCODER);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*gr_set_producer_usage_)(gralloc1_dev_, descriptor, GRALLOC1_PRODUCER_USAGE_CPU_WRITE);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*gr_set_dimensions_)(gralloc1_dev_, descriptor, width, height);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*gr_set_format_)(gralloc1_dev_, descriptor, HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

        gr1_err = (*gr_allocate_)(gralloc1_dev_, 1, &descriptor, handle);
        if (GRALLOC1_ERROR_NONE != gr1_err) break;

    } while(false);

    if (0 != descriptor) {
        (*gr_destroy_descriptor_)(gralloc1_dev_, descriptor);
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

c2_status_t MfxGrallocAllocator::Free(const buffer_handle_t handle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    MFX_DEBUG_TRACE_P(handle);

    if (handle) {
        int32_t gr1_err = (*gr_release_)(gralloc1_dev_, handle);
        if (GRALLOC1_ERROR_NONE != gr1_err)
        {
            MFX_DEBUG_TRACE_I32(gr1_err);
            res = C2_BAD_VALUE;
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxGrallocAllocator::LockFrame(buffer_handle_t handle, uint8_t** data, C2PlanarLayout *layout)
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

        int32_t err = (*gr_lock_)(gralloc1_dev_,
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

c2_status_t MfxGrallocAllocator::UnlockFrame(buffer_handle_t handle)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_P(handle);
    c2_status_t res = C2_OK;

    int32_t releaseFence = -1;
    int32_t gr1_res = (*gr_unlock_)(gralloc1_dev_, (buffer_handle_t)handle, &releaseFence);
    if (GRALLOC1_ERROR_NONE != gr1_res)
    {
        MFX_DEBUG_TRACE_I32(gr1_res);
        res = C2_BAD_STATE;
    }
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}
