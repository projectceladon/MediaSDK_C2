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

#include <mutex>

struct VaMemId
{
    VASurfaceID* surface_;
    VAImage image_;
    mfxU32 fourcc_;
};

class MfxVaFrameAllocator : public MfxFrameAllocator
{
public:
    MfxVaFrameAllocator(VADisplay dpy);
    virtual ~MfxVaFrameAllocator();

private:
    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *frame_data);
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *frame_data);
    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle);
    virtual mfxStatus AllocImpl(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response);
    virtual mfxStatus FreeImpl(mfxFrameAllocResponse *response);

private:
    VADisplay dpy_;

    std::mutex mutex_;

    MFX_CLASS_NO_COPY(MfxVaFrameAllocator)
};

#endif //#if defined(LIBVA_SUPPORT)
