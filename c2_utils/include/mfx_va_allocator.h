/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#if defined(LIBVA_SUPPORT)

#include "mfx_defs.h"
#include "mfx_allocator.h"
#include "mfx_frame_converter.h"
#include "mfx_gralloc_allocator.h"

#include <mutex>
#include <map>

// This struct contains pointer to VASurface allocated somewhere else.
struct VaMemId
{
    // Can also store VABufferID* for P8 format
    VASurfaceID* surface_;
    VAImage image_;
    mfxU32 fourcc_;
    buffer_handle_t gralloc_buffer_;
};

class MfxVaFrameAllocator : public MfxFrameAllocator, public MfxFrameConverter
{
public:
    MfxVaFrameAllocator(VADisplay dpy);
    virtual ~MfxVaFrameAllocator();

private: // MfxFrameAllocator
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) override;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response) override;
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *frame_data) override;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *frame_data) override;
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle) override;

private: // MfxFrameConverter
    virtual mfxStatus ConvertGrallocToVa(buffer_handle_t gralloc_buffer_, bool decode_target,
        mfxMemId* mem_id) override;
    virtual void FreeGrallocToVaMapping(buffer_handle_t gralloc_buffer) override;
    virtual void FreeGrallocToVaMapping(mfxMemId mem_id) override;
    virtual void FreeAllMappings() override;

private:
    mfxStatus MapGrallocBufferToSurface(buffer_handle_t gralloc_buffer, bool decode_target,
        mfxU32* fourcc, VASurfaceID* surface);

    mfxStatus CreateNV12SurfaceFromGralloc(buffer_handle_t gralloc_buffer, bool decode_target,
        const MfxGrallocModule::BufferDetails& buffer_details, VASurfaceID* surface);

    void FreeMemId();
private:
    // This is extension of VaMemId having allocated VASurface.
    struct VaMemIdAllocated
    {
        VaMemId mem_id;
        VASurfaceID surface_;
    };

    typedef std::function<void(VaMemIdAllocated*)> VaMemIdDeleter;

private:
    VADisplay dpy_;

    std::mutex mutex_;

    std::unique_ptr<MfxGrallocModule> gralloc_module_; // lazy init

    std::map<buffer_handle_t, std::unique_ptr<VaMemIdAllocated, VaMemIdDeleter>>
        mapped_va_surfaces_;

    MFX_CLASS_NO_COPY(MfxVaFrameAllocator)
};

#endif //#if defined(LIBVA_SUPPORT)
