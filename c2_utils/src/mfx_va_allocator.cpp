/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#if defined(LIBVA_SUPPORT)

#include "mfx_va_allocator.h"

#include "mfx_defs.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "va/va_android.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_va_allocator"

#define va_to_mfx_status(sts) ((VA_STATUS_SUCCESS == sts) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN)

static unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc) {
        case MFX_FOURCC_NV12:
            return VA_FOURCC_NV12;
        default:
            return 0;
    }
}

MfxVaFrameAllocator::MfxVaFrameAllocator(VADisplay dpy)
    : dpy_(dpy)
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxVaFrameAllocator::~MfxVaFrameAllocator()
{
    MFX_DEBUG_TRACE_FUNC;
}

mfxStatus MfxVaFrameAllocator::AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(mutex_);

    mfxStatus mfx_res = MFX_ERR_NONE;
    *response = mfxFrameAllocResponse {};

    do {

        unsigned int va_fourcc = ConvertMfxFourccToVAFormat(request->Info.FourCC);
        if (!va_fourcc) {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
            break;
        }

        mfxU16 surfaces_count = request->NumFrameSuggested;
        if (!surfaces_count) {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
            break;
        }

        std::unique_ptr<VASurfaceID[]> surfaces { new (std::nothrow)VASurfaceID[surfaces_count] };
        std::unique_ptr<VaMemId[]> va_mids { new (std::nothrow)VaMemId[surfaces_count] };
        std::unique_ptr<mfxMemId[]> mids { new (std::nothrow)mfxMemId[surfaces_count] };

        if (!surfaces || !va_mids || !mids) {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
            break;
        }

        VASurfaceAttrib attrib;
        attrib.type = VASurfaceAttribPixelFormat;
        attrib.value.type = VAGenericValueTypeInteger;
        attrib.value.value.i = va_fourcc;
        attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;

        VAStatus va_res = vaCreateSurfaces(dpy_, VA_RT_FORMAT_YUV420,
            request->Info.Width, request->Info.Height,
            surfaces.get(), surfaces_count, &attrib, 1);
        if (VA_STATUS_SUCCESS != va_res) {
            mfx_res = va_to_mfx_status(va_res);
            break;
        }

        for (int i = 0; i < surfaces_count; ++i)
        {
            va_mids[i].fourcc_ = request->Info.FourCC;
            va_mids[i].surface_ = &(surfaces[i]);
            mids[i] = &va_mids[i];
        }

        surfaces.release();
        va_mids.release();
        response->mids = mids.release();
        response->NumFrameActual = surfaces_count;

    } while(false);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFrameAllocator::FreeImpl(mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    if (response) {

        if (response->mids)
        {
            VaMemId* va_mids = (VaMemId*)(response->mids[0]);
            VASurfaceID* surfaces = va_mids->surface_;

            delete[] va_mids;
            delete[] response->mids;
            response->mids = NULL;

            vaDestroySurfaces(dpy_, surfaces, response->NumFrameActual);
            delete[] surfaces;
        }

        response->NumFrameActual = 0;
    } else {
        mfx_res = MFX_ERR_NULL_PTR;
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

static mfxStatus InitMfxFrameData(VaMemId& va_mid, mfxU8* pBuffer, mfxFrameData *frame_data)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    frame_data->MemId = &va_mid;

    switch (va_mid.image_.format.fourcc) {
    case VA_FOURCC_NV12:
        if (va_mid.fourcc_ == MFX_FOURCC_NV12) {
            frame_data->Pitch = (mfxU16)va_mid.image_.pitches[0];
            frame_data->Y = pBuffer + va_mid.image_.offsets[0];
            frame_data->U = pBuffer + va_mid.image_.offsets[1];
            frame_data->V = frame_data->U + 1;
        }
        else mfx_res = MFX_ERR_LOCK_MEMORY;
        break;
    default:
        mfx_res = MFX_ERR_LOCK_MEMORY;
        break;
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFrameAllocator::LockFrame(mfxMemId mid, mfxFrameData *frame_data)
{
    MFX_DEBUG_TRACE_FUNC;
    std::lock_guard<std::mutex> lock(mutex_);

    mfxStatus mfx_res = MFX_ERR_NONE;

    do {

        VaMemId* va_mid = (VaMemId*)mid;
        if (!va_mid || !(va_mid->surface_)) {
            mfx_res = MFX_ERR_INVALID_HANDLE;
            break;
        }

        VAStatus va_res = vaDeriveImage(dpy_, *(va_mid->surface_), &(va_mid->image_));
        if (VA_STATUS_SUCCESS != va_res) {
            mfx_res = va_to_mfx_status(va_res);
            break;
        }

        va_res = vaSyncSurface(dpy_, *(va_mid->surface_));
        if (VA_STATUS_SUCCESS != va_res) {
            mfx_res = va_to_mfx_status(va_res);
            break;
        }

        mfxU8* pBuffer = 0;
        va_res = vaMapBuffer(dpy_, va_mid->image_.buf, (void**)&pBuffer);
        if (VA_STATUS_SUCCESS != va_res) {
            mfx_res = va_to_mfx_status(va_res);
            break;
        }

        mfx_res = InitMfxFrameData(*va_mid, pBuffer, frame_data);

    } while(false);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *frame_data)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    std::lock_guard<std::mutex> lock(mutex_);

    VaMemId* va_mid = (VaMemId*)mid;
    if (va_mid && va_mid->surface_) {
        vaUnmapBuffer(dpy_, va_mid->image_.buf);
        vaDestroyImage(dpy_, va_mid->image_.image_id);

        if (nullptr != frame_data) {
            frame_data->Pitch = 0;
            frame_data->Y = nullptr;
            frame_data->U = nullptr;
            frame_data->V = nullptr;
            frame_data->A = nullptr;
        }
    } else {
        mfx_res = MFX_ERR_INVALID_HANDLE;
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFrameAllocator::GetFrameHDL(mfxMemId mid, mfxHDL *handle)
{
    MFX_DEBUG_TRACE_FUNC;
    std::lock_guard<std::mutex> lock(mutex_);

    VaMemId* va_mid = (VaMemId*)mid;

    if (!handle || !va_mid || !(va_mid->surface_)) return MFX_ERR_INVALID_HANDLE;

    *handle = va_mid->surface_; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

#endif // #if defined(LIBVA_SUPPORT)
