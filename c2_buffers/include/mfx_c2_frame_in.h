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

#include "mfx_frame_converter.h"
#include "mfx_defs.h"
#include "mfx_c2_vpp_wrapp.h"

#include <C2Buffer.h>
#include <C2Work.h>

class MfxC2FrameIn
{
public:
    MfxC2FrameIn() = default;
    MfxC2FrameIn(MfxC2FrameIn&& other) = default;
    ~MfxC2FrameIn();

    c2_status_t init(std::shared_ptr<MfxFrameConverter> frame_converter,  std::unique_ptr<const C2GraphicView> c_graph_view,
        C2FrameData& buf_pack, mfxFrameSurface1 *mfx_frame);

    c2_status_t init(std::shared_ptr<MfxFrameConverter> frame_converter,
        C2FrameData& buf_pack, const mfxFrameInfo& info, c2_nsecs_t timeout);

    mfxFrameSurface1* GetMfxFrameSurface() const
    {
        return mfx_frame_surface_;
    }
private:
    std::shared_ptr<C2Buffer> c2_buffer_;
    std::unique_ptr<const C2GraphicView> c2_graphic_view_;
    mfxFrameSurface1 *mfx_frame_surface_;
    std::shared_ptr<uint8_t> yuv_data_; //only for sw frame
    std::shared_ptr<MfxFrameConverter> frame_converter_;
};
