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
#include <memory>

class MfxFramePoolAllocator
{
public:
    virtual void SetC2Allocator(std::shared_ptr<C2BlockPool> c2_allocator) = 0;
    virtual std::shared_ptr<C2GraphicBlock> Alloc() = 0;
    virtual void Reset() = 0;
    virtual bool InCache(uint64_t id) = 0;
    virtual void SetBufferCount(unsigned int cnt) = 0;
    virtual void SetConsumerUsage(uint64_t usage) = 0;

protected: // virtual deletion prohibited
    virtual ~MfxFramePoolAllocator() = default;
};
