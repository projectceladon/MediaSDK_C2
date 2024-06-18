// Copyright (c) 2017-2022 Intel Corporation
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

#include <C2Buffer.h>
#include <C2Work.h>

#include "mfx_defs.h"
#include "mfx_frame_constructor.h"

class MfxC2BitstreamIn
{
public:
    MfxC2BitstreamIn(MfxC2FrameConstructorType fc_type);
    virtual ~MfxC2BitstreamIn();

    virtual c2_status_t Reset();

    virtual c2_status_t Unload();

    virtual std::shared_ptr<IMfxC2FrameConstructor> GetFrameConstructor() { return m_frameConstructor; }
    // Maps c2 linear block and can leave it in mapped state until
    // frame_view freed or frame_view->Release is called.
    virtual c2_status_t AppendFrame(const C2FrameData& buf_pack, c2_nsecs_t timeout,
        std::unique_ptr<C2ReadView>* view);

    virtual bool IsInReset();
protected: // variables
    std::shared_ptr<IMfxC2FrameConstructor> m_frameConstructor;

private:
    MFX_CLASS_NO_COPY(MfxC2BitstreamIn)
};
