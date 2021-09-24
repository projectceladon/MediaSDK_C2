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

#ifdef LIBVA_SUPPORT

#include "mfx_dev.h"
#include "mfx_va_allocator.h"
#include "mfx_va_frame_pool_allocator.h"

#define MFX_VA_ANDROID_DISPLAY_ID 0x18c34078

class MfxDevVa : public MfxDev
{
public:
    MfxDevVa(Usage usage);
    virtual ~MfxDevVa();

public:
    VADisplay GetVaDisplay() { return va_display_; }

private:
    mfxStatus Init() override;
    mfxStatus Close() override;
    std::shared_ptr<MfxFrameAllocator> GetFrameAllocator() override;
    std::shared_ptr<MfxFrameConverter> GetFrameConverter() override;
    std::shared_ptr<MfxFramePoolAllocator> GetFramePoolAllocator() override;
#ifdef USE_ONEVPL
    mfxStatus InitMfxSession(mfxSession session) override;
#else
    mfxStatus InitMfxSession(MFXVideoSession* session) override;
#endif

protected:
    typedef unsigned int MfxVaAndroidDisplayId;

protected:
    Usage usage_ {};
    bool va_initialized_ { false };
    MfxVaAndroidDisplayId display_id_ = MFX_VA_ANDROID_DISPLAY_ID;
    VADisplay va_display_ { nullptr };
    std::shared_ptr<MfxVaFrameAllocator> va_allocator_;
    std::shared_ptr<MfxVaFramePoolAllocator> va_pool_allocator_;

private:
    MFX_CLASS_NO_COPY(MfxDevVa)
};

#endif // #ifdef LIBVA_SUPPORT
