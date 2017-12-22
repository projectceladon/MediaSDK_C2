/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_allocator.h"

inline MfxFrameAllocator* WrapperGetAllocator(mfxHDL pthis)
{
    return (MfxFrameAllocator*)pthis;
}

static mfxStatus WrapperAlloc(
  mfxHDL pthis, mfxFrameAllocRequest *request, mfxFrameAllocResponse *response)
{
    MfxFrameAllocator* a = WrapperGetAllocator(pthis);
    return (a) ? a->AllocFrames(request, response) : MFX_ERR_INVALID_HANDLE;
}

static mfxStatus WrapperLock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    MfxFrameAllocator* a = WrapperGetAllocator(pthis);
    return (a) ? a->LockFrame(mid, ptr) : MFX_ERR_INVALID_HANDLE;
}

static mfxStatus WrapperUnlock(mfxHDL pthis, mfxMemId mid, mfxFrameData *ptr)
{
    MfxFrameAllocator* a = WrapperGetAllocator(pthis);
    return (a) ? a->UnlockFrame(mid, ptr) : MFX_ERR_INVALID_HANDLE;
}

static mfxStatus WrapperFree(mfxHDL pthis, mfxFrameAllocResponse *response)
{
    MfxFrameAllocator* a = WrapperGetAllocator(pthis);
    return (a) ? a->FreeFrames(response) : MFX_ERR_INVALID_HANDLE;
}

static mfxStatus WrapperGetHdl(mfxHDL pthis, mfxMemId mid, mfxHDL *handle)
{
    MfxFrameAllocator* a = WrapperGetAllocator(pthis);
    return (a) ? a->GetFrameHDL(mid, handle) : MFX_ERR_INVALID_HANDLE;
}


MfxFrameAllocator::MfxFrameAllocator()
{
    mfx_allocator_.pthis = this;
    mfx_allocator_.Alloc = WrapperAlloc;
    mfx_allocator_.Free = WrapperFree;
    mfx_allocator_.Lock = WrapperLock;
    mfx_allocator_.Unlock = WrapperUnlock;
    mfx_allocator_.GetHDL = WrapperGetHdl;
}

MfxFrameAllocator::~MfxFrameAllocator()
{
}
