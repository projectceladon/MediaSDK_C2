/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"

class MfxFrameAllocator
{
private:
    mfxFrameAllocator mfx_allocator_ {};
public:
    MfxFrameAllocator();
    virtual ~MfxFrameAllocator();
    mfxFrameAllocator& GetMfxAllocator() { return mfx_allocator_; }

public:
    virtual mfxStatus AllocFrames(mfxFrameAllocRequest *request, mfxFrameAllocResponse *response) = 0;
    virtual mfxStatus FreeFrames(mfxFrameAllocResponse *response) = 0;

    virtual mfxStatus LockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;
    virtual mfxStatus UnlockFrame(mfxMemId mid, mfxFrameData *ptr) = 0;

    virtual mfxStatus GetFrameHDL(mfxMemId mid, mfxHDL *handle) = 0;

private:
    MFX_CLASS_NO_COPY(MfxFrameAllocator)
};
