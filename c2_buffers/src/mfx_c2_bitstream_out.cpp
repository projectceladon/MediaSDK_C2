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

#include "mfx_c2_bitstream_out.h"
#include "mfx_debug.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_debug.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_bitstream_out"

static void InitMfxBitstream(
    uint8_t* raw, uint32_t capacity, mfxBitstream* mfx_bitstream)
{
    MFX_DEBUG_TRACE_FUNC;

    memset(mfx_bitstream, 0, sizeof(mfxBitstream));

    mfx_bitstream->Data = raw;
    mfx_bitstream->MaxLength = capacity;
}

c2_status_t MfxC2BitstreamOut::Create(
    std::shared_ptr<C2LinearBlock> block, c2_nsecs_t timeout,
    MfxC2BitstreamOut* wrapper)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {

        if (nullptr == wrapper) {
            res = C2_BAD_VALUE;
            break;
        }

        res = MapLinearBlock(*block, timeout, &wrapper->c2_linear_view_);
        if (C2_OK != res) break;

        wrapper->mfx_bitstream_ = std::make_unique<mfxBitstream>();
        wrapper->c2_linear_block_ = block;

        InitMfxBitstream(wrapper->c2_linear_view_->data(), block->capacity(), wrapper->mfx_bitstream_.get());

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}
