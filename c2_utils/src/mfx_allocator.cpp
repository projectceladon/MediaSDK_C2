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
    m_mfxAllocator.pthis = this;
    m_mfxAllocator.Alloc = WrapperAlloc;
    m_mfxAllocator.Free = WrapperFree;
    m_mfxAllocator.Lock = WrapperLock;
    m_mfxAllocator.Unlock = WrapperUnlock;
    m_mfxAllocator.GetHDL = WrapperGetHdl;
}

MfxFrameAllocator::~MfxFrameAllocator()
{
}
