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
#include <mfx_defs.h>
#include <C2Buffer.h>

class IMfxGrallocModule
{
public:
    virtual ~IMfxGrallocModule() = default;

    virtual c2_status_t Init() = 0;

    struct BufferDetails
    {
        buffer_handle_t handle;
        int32_t prime;
        int width;
        int height;
        int format;
        uint32_t planes_count;
        uint32_t pitches[C2PlanarLayout::MAX_NUM_PLANES];// pitch for each plane
        uint32_t allocWidth;
        uint32_t allocHeight;
        BufferDetails():
            handle(nullptr),
            prime(-1),
            width(0),
            height(0),
            format(0),
            planes_count(0),
            pitches{},
            allocWidth(0),
            allocHeight(0)
        {}
    };

    virtual c2_status_t GetBufferDetails(const buffer_handle_t handle, BufferDetails* details) = 0;
    virtual c2_status_t GetBackingStore(const buffer_handle_t rawHandle, uint64_t *id) = 0;

    virtual buffer_handle_t ImportBuffer(const buffer_handle_t rawHandle) = 0;
};