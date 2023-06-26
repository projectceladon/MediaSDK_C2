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

#if defined(LIBVA_SUPPORT)

#include "mfx_va_allocator.h"

#include "mfx_defs.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "va/va_android.h"
#include "va/va_drmcommon.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_va_allocator"

#define va_to_mfx_status(sts) ((VA_STATUS_SUCCESS == sts) ? MFX_ERR_NONE : MFX_ERR_UNKNOWN)

#define IS_PRIME_VALID(prime) ((prime >= 0) ? true : false)

static unsigned int ConvertMfxFourccToVAFormat(mfxU32 fourcc)
{
    switch (fourcc) {
        case MFX_FOURCC_NV12:
            return VA_FOURCC_NV12;
        case MFX_FOURCC_P010:
            return VA_FOURCC_P010;
        case MFX_FOURCC_P8:
            return VA_FOURCC_P208;
        case MFX_FOURCC_YV12:
            return VA_FOURCC_YV12;
        default:
            return 0;
    }
}

static mfxU32 ConvertVAFourccToMfxFormat(unsigned int fourcc)
{
    switch (fourcc)
    {
        case VA_FOURCC_NV12:
            return MFX_FOURCC_NV12;
        case VA_FOURCC_P010:
            return MFX_FOURCC_P010;
        case VA_FOURCC_RGBA:
        case VA_FOURCC_BGRA:
        case VA_FOURCC_RGBX:
            return MFX_FOURCC_RGB4;
        case VA_FOURCC_YV12:
            return MFX_FOURCC_YV12;
        default:
            return 0;
    }
}

static unsigned int ConvertGrallocFourccToVAFormat(int fourcc)
{
    switch (fourcc)
    {
        case HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL:
        case HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL:
            return VA_FOURCC_NV12;
        case HAL_PIXEL_FORMAT_P010_INTEL:
            return VA_FOURCC_P010;
        case HAL_PIXEL_FORMAT_RGBA_8888:
            return VA_FOURCC_RGBA;
        case HAL_PIXEL_FORMAT_RGBX_8888:
            return VA_FOURCC_RGBX;
        case HAL_PIXEL_FORMAT_BGRA_8888:
            return VA_FOURCC_BGRA;
        case HAL_PIXEL_FORMAT_YV12:
            return VA_FOURCC_YV12;
        default:
            return 0;
    }
}


static mfxU32 ConvertVAFourccToVARTFormat(mfxU32 va_fourcc)
{
    switch (va_fourcc)
    {
        case VA_FOURCC_NV12:
        case VA_FOURCC_YV12:
            return VA_RT_FORMAT_YUV420;
        case VA_FOURCC_RGBA:
        case VA_FOURCC_BGRA:
        case VA_FOURCC_RGBX:
            return VA_RT_FORMAT_RGB32;
        case VA_FOURCC_P010:
            return VA_RT_FORMAT_YUV420_10;
        default:
            return 0;
    }
}

MfxVaFrameAllocator::MfxVaFrameAllocator(VADisplay dpy)
    : m_dpy(dpy)
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxVaFrameAllocator::~MfxVaFrameAllocator()
{
    MFX_DEBUG_TRACE_FUNC;
}

mfxStatus MfxVaFrameAllocator::AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_U32(request->Info.FourCC);
    MFX_DEBUG_TRACE_U32(request->Type);
    MFX_DEBUG_TRACE_I32(request->NumFrameMin);
    MFX_DEBUG_TRACE_I32(request->NumFrameSuggested);

    std::lock_guard<std::mutex> lock(m_mutex);

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

        if (VA_FOURCC_P208 != va_fourcc) {

            VASurfaceAttrib attrib;
            attrib.type = VASurfaceAttribPixelFormat;
            attrib.value.type = VAGenericValueTypeInteger;
            attrib.value.value.i = va_fourcc;
            attrib.flags = VA_SURFACE_ATTRIB_SETTABLE;

            VAStatus va_res = vaCreateSurfaces(m_dpy, VA_RT_FORMAT_YUV420,
                request->Info.Width, request->Info.Height,
                surfaces.get(), surfaces_count, &attrib, 1);
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                break;
            }
        } else {
            VAContextID context_id = request->AllocId;
            MFX_DEBUG_TRACE_U32(context_id);
            mfxU32 codedbuf_size = EstimatedEncodedFrameLen(request->Info.Width, request->Info.Height);

            VAStatus va_res = VA_STATUS_SUCCESS;
            for (int alloc_count = 0; alloc_count < surfaces_count; ++alloc_count) {

                va_res = vaCreateBuffer(m_dpy, context_id, VAEncCodedBufferType,
                    codedbuf_size, 1, nullptr, &(surfaces[alloc_count]));

                if (VA_STATUS_SUCCESS != va_res) {
                    MFX_DEBUG_TRACE_U32(va_res);
                    for (int i = alloc_count - 1; i >= 0; --i) { // free allocated already
                        vaDestroyBuffer(m_dpy, surfaces[i]);
                    }
                    break;
                }
            }

            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                break;
            }
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

mfxStatus MfxVaFrameAllocator::FreeFrames(mfxFrameAllocResponse *response)
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(m_mutex);

    mfxStatus mfx_res = MFX_ERR_NONE;
    if (response) {

        if (response->mids)
        {
            VaMemId* va_mids = (VaMemId*)(response->mids[0]);
            VASurfaceID* surfaces = va_mids->surface_;

            if (MFX_FOURCC_P8 != va_mids->fourcc_) {
                vaDestroySurfaces(m_dpy, surfaces, response->NumFrameActual);
            } else {
                for (int i = 0; i < response->NumFrameActual; ++i) {
                    vaDestroyBuffer(m_dpy, surfaces[i]);
                }
            }
            delete[] surfaces;
            delete[] va_mids;
            delete[] response->mids;
            response->mids = NULL;
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

    *frame_data = mfxFrameData {};
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
    std::lock_guard<std::mutex> lock(m_mutex);

    mfxStatus mfx_res = MFX_ERR_NONE;

    do {

        VaMemId* va_mid = (VaMemId*)mid;
        if (!va_mid || !(va_mid->surface_)) {
            mfx_res = MFX_ERR_INVALID_HANDLE;
            break;
        }

        if (MFX_FOURCC_P8 == va_mid->fourcc_)   // bitstream processing
        {
            VACodedBufferSegment *coded_buffer_segment;
            VAStatus va_res = vaMapBuffer(m_dpy, *(va_mid->surface_),
                (void **)(&coded_buffer_segment));
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                break;
            }
            *frame_data = mfxFrameData {};
            frame_data->MemId = va_mid;
            frame_data->Y = (mfxU8*)coded_buffer_segment->buf;
        } else {
            VAStatus va_res = vaDeriveImage(m_dpy, *(va_mid->surface_), &(va_mid->image_));
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                break;
            }

            va_res = vaSyncSurface(m_dpy, *(va_mid->surface_));
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                break;
            }

            mfxU8* pBuffer = nullptr;
            va_res = vaMapBuffer(m_dpy, va_mid->image_.buf, (void**)&pBuffer);
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                break;
            }
            mfx_res = InitMfxFrameData(*va_mid, pBuffer, frame_data);
        }


    } while(false);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFrameAllocator::UnlockFrame(mfxMemId mid, mfxFrameData *frame_data)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    std::lock_guard<std::mutex> lock(m_mutex);

    VaMemId* va_mid = (VaMemId*)mid;
    if (va_mid && va_mid->surface_) {

        if (MFX_FOURCC_P8 == va_mid->fourcc_) { // bitstream processing
            VAStatus va_res = vaUnmapBuffer(m_dpy, *(va_mid->surface_));
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                return mfx_res;
            }
        }
        else { // Image processing
            VAStatus va_res = vaUnmapBuffer(m_dpy, va_mid->image_.buf);
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                return mfx_res;
            }
            va_res = vaDestroyImage(m_dpy, va_mid->image_.image_id);
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                return mfx_res;
            }

            if (nullptr != frame_data) {
                frame_data->Pitch = 0;
                frame_data->Y = nullptr;
                frame_data->U = nullptr;
                frame_data->V = nullptr;
                frame_data->A = nullptr;
            }
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
    std::lock_guard<std::mutex> lock(m_mutex);

    VaMemId* va_mid = (VaMemId*)mid;

    if (!handle || !va_mid || !(va_mid->surface_)) return MFX_ERR_INVALID_HANDLE;

    *handle = va_mid->surface_; //VASurfaceID* <-> mfxHDL
    return MFX_ERR_NONE;
}

mfxStatus MfxVaFrameAllocator::ConvertGrallocToVa(buffer_handle_t gralloc_buffer, bool decode_target, mfxMemId* mem_id)
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(m_mutex);

    mfxStatus mfx_res = MFX_ERR_NONE;

    VASurfaceID surface { VA_INVALID_ID };

    do {
        if (!gralloc_buffer) {
            mfx_res = MFX_ERR_INVALID_HANDLE;
            break;
        }

        uint64_t id;
        if(C2_OK != MfxGrallocInstance::getInstance()->GetBackingStore(gralloc_buffer, &id)) {
            mfx_res = MFX_ERR_INVALID_HANDLE;
            break;
        }

        auto found = m_mappedVaSurfaces.find(id);
        if (found != m_mappedVaSurfaces.end()) {
            *mem_id = found->second.get();
        } else {
            mfxU32 fourcc {};
            mfx_res = MapGrallocBufferToSurface(gralloc_buffer, decode_target, &fourcc, &surface);
            if (MFX_ERR_NONE != mfx_res) break;

            VaMemIdAllocated* mem_id_alloc_raw = new (std::nothrow)VaMemIdAllocated();
            if (!mem_id_alloc_raw) {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
                break;
            }

            VaMemIdDeleter deleter = [this] (VaMemIdAllocated* va_mid) {
                if (VA_INVALID_ID != va_mid->surface_) {
                    vaDestroySurfaces(m_dpy, &va_mid->surface_, 1);
                }
            };

            std::unique_ptr<VaMemIdAllocated, VaMemIdDeleter> mem_id_alloc(mem_id_alloc_raw, deleter);

            mem_id_alloc->surface_ = surface; // Save VASurfaceID there

            // Save pointer to allocated VASurfaceID, needed for compatibility with VA surfaces
            // allocated through mfxFrameAllocator interface, they are allocated in straight array,
            // accessed with VaMemId.surface_ pointer.
            mem_id_alloc->mem_id.surface_ = &mem_id_alloc->surface_;
            mem_id_alloc->mem_id.fourcc_ = fourcc;
            mem_id_alloc->mem_id.gralloc_buffer_ = gralloc_buffer;

            *mem_id = mem_id_alloc.get();

            m_mappedVaSurfaces.emplace(id, std::move(mem_id_alloc));

            MFX_DEBUG_TRACE_STREAM(NAMED(this) << NAMED(gralloc_buffer) << NAMED(*mem_id) << NAMED(m_mappedVaSurfaces.size()));
        }
    } while(false);

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

void MfxVaFrameAllocator::FreeGrallocToVaMapping(mfxMemId mem_id)
{
    MFX_DEBUG_TRACE_FUNC;
    std::lock_guard<std::mutex> lock(m_mutex);

    auto match = [mem_id] (const auto& item) -> bool {
        return item.second.get() == (VaMemIdAllocated*)mem_id;
    };

    auto found = std::find_if(m_mappedVaSurfaces.begin(), m_mappedVaSurfaces.end(), match);
    if (found != m_mappedVaSurfaces.end()) {
        m_mappedVaSurfaces.erase(found);
    }
}

void MfxVaFrameAllocator::FreeAllMappings()
{
    MFX_DEBUG_TRACE_FUNC;
    std::lock_guard<std::mutex> lock(m_mutex);

    m_mappedVaSurfaces.clear();
}

mfxStatus MfxVaFrameAllocator::CreateSurfaceFromGralloc(const IMfxGrallocModule::BufferDetails& buffer_details,
    bool decode_target,
    VASurfaceID* surface)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    const IMfxGrallocModule::BufferDetails & info = buffer_details;
    MFX_DEBUG_TRACE_P(info.handle);
    MFX_DEBUG_TRACE_I32(info.prime);
    MFX_DEBUG_TRACE_I32(info.width);
    MFX_DEBUG_TRACE_I32(info.height);
    MFX_DEBUG_TRACE_I32(info.allocWidth);
    MFX_DEBUG_TRACE_I32(info.allocHeight);
    MFX_DEBUG_TRACE_U32(info.planes_count);
    for (uint32_t i = 0; i < C2PlanarLayout::MAX_NUM_PLANES; i++) {
        MFX_DEBUG_TRACE_U32(info.pitches[i]);
    }


    mfxU32 width = decode_target ? info.allocWidth : info.width;
    mfxU32 height = decode_target ? info.allocHeight : info.height;

    mfxU32 va_fourcc = ConvertGrallocFourccToVAFormat(info.format);
    mfxU32 rt_format = ConvertVAFourccToVARTFormat(va_fourcc);

    VASurfaceAttrib attribs[2];
    MFX_ZERO_MEMORY(attribs);

    VASurfaceAttribExternalBuffers surfExtBuf;
    MFX_ZERO_MEMORY(surfExtBuf);

    surfExtBuf.pixel_format = va_fourcc;
    surfExtBuf.width = width;
    surfExtBuf.height = height;
    surfExtBuf.pitches[0] = info.pitches[0];
    surfExtBuf.pitches[1] = info.pitches[1];
    surfExtBuf.pitches[2] = 0;
    surfExtBuf.pitches[3] = 0;
    surfExtBuf.offsets[0] = 0;
    // Gralloc buffer has been aligned with 32 pixels for decoder
    surfExtBuf.offsets[1] = decode_target ? info.pitches[0] * ((height + 31) & ~31) : info.pitches[0] * ((height + 15) & ~15);
    surfExtBuf.offsets[2] = 0;
    surfExtBuf.offsets[3] = 0;
    surfExtBuf.data_size = decode_target ? info.pitches[0] * ((height + 31) & ~31) * 1.5 : info.pitches[0] * ((height + 15) & ~15) * 1.5;
    surfExtBuf.num_planes = info.planes_count;
    surfExtBuf.num_buffers = 1;
    if (IS_PRIME_VALID(info.prime)) {
        surfExtBuf.buffers = (uintptr_t *)&info.prime;
        surfExtBuf.flags = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    } else {
        surfExtBuf.buffers = (uintptr_t *)&info.handle;
        surfExtBuf.flags = VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC;
    }

    attribs[0].type = (VASurfaceAttribType)VASurfaceAttribMemoryType;
    attribs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[0].value.type = VAGenericValueTypeInteger;
    if (IS_PRIME_VALID(info.prime)) {
        attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME;
    } else  {
        attribs[0].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_ANDROID_GRALLOC;
    }
    attribs[1].type = (VASurfaceAttribType)VASurfaceAttribExternalBufferDescriptor;
    attribs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
    attribs[1].value.type = VAGenericValueTypePointer;
    attribs[1].value.value.p = (void *)&surfExtBuf;

    VAStatus va_res = vaCreateSurfaces(m_dpy, rt_format,
        width, height,
        surface, 1,
        attribs, MFX_GET_ARRAY_SIZE(attribs));
    mfx_res = va_to_mfx_status(va_res);

    if (MFX_ERR_NONE == mfx_res)
    {
        // workaround for a 4k/8k playback performance issue
        if (info.width >= WIDTH_2K || info.height >= HEIGHT_2K)
        mfx_res = TouchSurface(*surface);
    }

    MFX_DEBUG_TRACE_I32(*surface);
    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFrameAllocator::MapGrallocBufferToSurface(buffer_handle_t gralloc_buffer, bool decode_target,
    mfxU32* fourcc, VASurfaceID* surface)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(gralloc_buffer);

    do {
        IMfxGrallocModule::BufferDetails buffer_details;
        c2_status_t sts = MfxGrallocInstance::getInstance()->GetBufferDetails(gralloc_buffer, &buffer_details);
        if(C2_OK != sts) {
            mfx_res = MFX_ERR_INVALID_HANDLE;
            break;
        }

        MFX_DEBUG_TRACE_STREAM(buffer_details.format);

        if (buffer_details.format == HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL ||
            buffer_details.format == HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL ||
            buffer_details.format == HAL_PIXEL_FORMAT_P010_INTEL ||
            buffer_details.format == HAL_PIXEL_FORMAT_RGBA_8888 ||
            buffer_details.format == HAL_PIXEL_FORMAT_RGBX_8888 ||
            buffer_details.format == HAL_PIXEL_FORMAT_BGRA_8888 ||
            buffer_details.format == HAL_PIXEL_FORMAT_YV12) {

            // on Android Q HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL works the same way
            // some other driveers might demand creation of separate VASurface and copy contents there.
            // see format difference in include/ufo/graphics.h
            mfx_res = CreateSurfaceFromGralloc(buffer_details, decode_target, surface);
            *fourcc = ConvertVAFourccToMfxFormat(ConvertGrallocFourccToVAFormat(buffer_details.format));
        } else {
            // Other formats are unsupported for now,
            // including HAL_PIXEL_FORMAT_NV12_LINEAR_CAMERA_INTEL which might
            // demand creation of separate VASurface and copy contents there. (It depends on driver).
            mfx_res = MFX_ERR_UNSUPPORTED;
        }
    } while(false);

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxStatus MfxVaFrameAllocator::TouchSurface(VASurfaceID surface)
{
    VAImage image;
    unsigned char* buffer;
    VAStatus va_res;
    mfxStatus mfx_res = MFX_ERR_NONE;

    if (VA_INVALID_ID == surface) return MFX_ERR_UNKNOWN;

    va_res = vaDeriveImage(m_dpy, surface, &image);
    if (VA_STATUS_SUCCESS == va_res)
    {
        va_res = vaMapBuffer(m_dpy, image.buf, (void **) &buffer);
        if (VA_STATUS_SUCCESS == va_res)
        {
            *buffer = 0x0; // can have any value
            va_res = vaUnmapBuffer(m_dpy, image.buf);
            if (VA_STATUS_SUCCESS != va_res) {
                mfx_res = va_to_mfx_status(va_res);
                return mfx_res;
            }
        }
        va_res = vaDestroyImage(m_dpy, image.image_id);
        if (VA_STATUS_SUCCESS != va_res) {
            mfx_res = va_to_mfx_status(va_res);
            return mfx_res;
        }
    }
    return mfx_res;
}
#endif // #if defined(LIBVA_SUPPORT)
