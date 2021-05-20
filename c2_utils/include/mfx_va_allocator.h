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

protected: // MfxFrameConverter
    virtual mfxStatus ConvertGrallocToVa(buffer_handle_t gralloc_buffer_, bool decode_target,
        mfxMemId* mem_id) override;
    virtual void FreeGrallocToVaMapping(mfxMemId mem_id) override;
    virtual void FreeAllMappings() override;

    std::unique_ptr<MfxGrallocAllocator> gralloc_allocator_;
private:
    mfxStatus MapGrallocBufferToSurface(buffer_handle_t gralloc_buffer, bool decode_target,
        mfxU32* fourcc, VASurfaceID* surface);

    mfxStatus CreateSurfaceFromGralloc(const MfxGrallocModule::BufferDetails& buffer_details,
        bool decode_target,
        VASurfaceID* surface);

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

    std::map<uint64_t, std::unique_ptr<VaMemIdAllocated, VaMemIdDeleter>>
        mapped_va_surfaces_;

    MFX_CLASS_NO_COPY(MfxVaFrameAllocator)
};

#endif //#if defined(LIBVA_SUPPORT)
