// Copyright (c) 2017-2019 Intel Corporation
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

#include "mfx_defs.h"
#include "mfx_allocator.h"
#include "mfx_frame_converter.h"
#include "mfx_frame_pool_allocator.h"

#include <memory>

class MfxDev
{
public:
    enum class Usage {
        Encoder,
        Decoder
    };
public:
    virtual ~MfxDev() = default;

public:
    virtual mfxStatus Init() = 0;

    virtual mfxStatus Close() = 0;

    virtual std::shared_ptr<MfxFrameAllocator> GetFrameAllocator() = 0;

    virtual std::shared_ptr<MfxFrameConverter> GetFrameConverter() = 0;

    virtual std::shared_ptr<MfxFramePoolAllocator> GetFramePoolAllocator() = 0;

    virtual bool CheckHUCSupport(VAProfile profile) = 0;

#ifdef USE_ONEVPL
    virtual mfxStatus InitMfxSession(mfxSession session) = 0;
#else
    virtual mfxStatus InitMfxSession(MFXVideoSession* session) = 0;
#endif
    static mfxStatus Create(Usage usage, std::unique_ptr<MfxDev>* device);
};
