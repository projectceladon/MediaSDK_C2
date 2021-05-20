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

#include <C2Buffer.h>
#include <C2Work.h>

#include "mfx_frame_converter.h"
#include "mfx_defs.h"

class MfxC2FrameOut
{
public:
    MfxC2FrameOut() = default;

    MfxC2FrameOut(std::shared_ptr<C2GraphicBlock>&& c2_block,
        std::shared_ptr<mfxFrameSurface1> mfx_frame)
        : c2_graphic_block_(std::move(c2_block))
        , mfx_surface_(mfx_frame)
    {}

    static c2_status_t Create(const std::shared_ptr<MfxFrameConverter>& frame_converter,
                                    std::shared_ptr<C2GraphicBlock> block,
                                    const mfxFrameInfo& info,
                                    c2_nsecs_t timeout,
                                    MfxC2FrameOut* wrapper,
                                    buffer_handle_t hndl);

    std::shared_ptr<C2GraphicBlock> GetC2GraphicBlock() const;
    std::shared_ptr<C2GraphicView> GetC2GraphicView() const;
    std::shared_ptr<mfxFrameSurface1> GetMfxFrameSurface() const;

    bool operator==(const MfxC2FrameOut& other) const {
        return
            c2_graphic_block_ == other.c2_graphic_block_ &&
            c2_graphic_view_ == other.c2_graphic_view_ &&
            mfx_surface_ == other.mfx_surface_;
    }
private:
    std::shared_ptr<C2GraphicBlock> c2_graphic_block_;
    std::shared_ptr<C2GraphicView> c2_graphic_view_;
    std::shared_ptr<mfxFrameSurface1> mfx_surface_;
};
