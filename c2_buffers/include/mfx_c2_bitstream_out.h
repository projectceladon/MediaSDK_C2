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

#include "mfx_defs.h"

class MfxC2BitstreamOut
{
public:
    MfxC2BitstreamOut() = default;

    static c2_status_t Create(
        std::shared_ptr<C2LinearBlock> block, c2_nsecs_t timeout,
        MfxC2BitstreamOut* wrapper);

    std::shared_ptr<C2LinearBlock> GetC2LinearBlock() const
    {
        return m_c2LinearBlock;
    }

    mfxBitstream* GetMfxBitstream() const
    {
        return m_mfxBitstream.get();
    }
private:
    std::shared_ptr<C2LinearBlock> m_c2LinearBlock;
    std::unique_ptr<C2WriteView> m_c2LinearView;
    std::unique_ptr<mfxBitstream> m_mfxBitstream;
};
