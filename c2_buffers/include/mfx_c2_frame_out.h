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
        : m_c2GraphicBlock(std::move(c2_block))
        , m_mfxSurface(std::move(mfx_frame))
    {}

    static c2_status_t Create(const std::shared_ptr<MfxFrameConverter>& frame_converter,
                                    std::shared_ptr<C2GraphicBlock> block,
                                    const mfxFrameInfo& info,
                                    MfxC2FrameOut* wrapper,
                                    buffer_handle_t hndl);

    static c2_status_t Create(std::shared_ptr<C2GraphicBlock> block,
                               const mfxFrameInfo& info,
                               c2_nsecs_t timeout,
                               MfxC2FrameOut* wrapper);

    std::shared_ptr<C2GraphicBlock> GetC2GraphicBlock() const;
    std::shared_ptr<C2GraphicView> GetC2GraphicView() const;
    std::shared_ptr<mfxFrameSurface1> GetMfxFrameSurface() const;

    bool operator==(const MfxC2FrameOut& other) const {
        return
            m_c2GraphicBlock == other.m_c2GraphicBlock &&
            m_c2GraphicView == other.m_c2GraphicView &&
            m_mfxSurface == other.m_mfxSurface;
    }
private:
    std::shared_ptr<C2GraphicBlock> m_c2GraphicBlock;
    std::shared_ptr<C2GraphicView> m_c2GraphicView;
    std::shared_ptr<mfxFrameSurface1> m_mfxSurface;
};
