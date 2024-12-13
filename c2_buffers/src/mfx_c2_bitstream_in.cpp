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

#include "mfx_c2_bitstream_in.h"
#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_msdk_debug.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_bitstream_in"

constexpr c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

MfxC2BitstreamIn::MfxC2BitstreamIn(MfxC2FrameConstructorType fc_type)
{
    MFX_DEBUG_TRACE_FUNC;

    m_frameConstructor = MfxC2FrameConstructorFactory::CreateFrameConstructor(fc_type);
}

MfxC2BitstreamIn::~MfxC2BitstreamIn()
{
    MFX_DEBUG_TRACE_FUNC;
}

c2_status_t MfxC2BitstreamIn::Reset()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        mfxStatus mfx_res = m_frameConstructor->Reset();
        res = MfxStatusToC2(mfx_res);
        if(C2_OK != res) break;
    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

bool MfxC2BitstreamIn::IsInReset()
{
    MFX_DEBUG_TRACE_FUNC;

    return m_frameConstructor->IsInReset();
}

c2_status_t MfxC2BitstreamIn::Unload()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        mfxStatus mfx_res = m_frameConstructor->Unload();
        res = MfxStatusToC2(mfx_res);
        if(C2_OK != res) break;
    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}


c2_status_t MfxC2BitstreamIn::AppendFrame(const C2FrameData& buf_pack, c2_nsecs_t timeout,
    std::unique_ptr<C2ReadView>* frame_view)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    const mfxU8* data = nullptr;
    mfxU32 filled_len = 0;

    std::unique_ptr<C2ConstLinearBlock> encryptedBlock;
    for (auto &infoBuffer: buf_pack.infoBuffers) {
        if (infoBuffer.index().typeIndex() == kParamIndexEncryptedBuffer) {
            const C2BufferData& buf_data = infoBuffer.data();
            encryptedBlock = std::make_unique<C2ConstLinearBlock>(buf_data.linearBlocks().front());
        }
    }

    const mfxU8* infobuffer = nullptr;
    std::unique_ptr<C2ReadView> bs_read_view;
    
    if (encryptedBlock != nullptr) {
        MapConstLinearBlock(*encryptedBlock, TIMEOUT_NS, &bs_read_view);
        infobuffer = bs_read_view->data() + encryptedBlock->offset();
        MFX_DEBUG_TRACE_STREAM("c2 infobuffer data: " << FormatHex(infobuffer, encryptedBlock->size()));
    }

    do {
        if (!frame_view) {
            res = C2_BAD_VALUE;
            break;
        }

        if (buf_pack.buffers.size() == 0) {
            m_frameConstructor->SetEosMode(buf_pack.flags & C2FrameData::FLAG_END_OF_STREAM);
            break;
        }

        std::unique_ptr<C2ConstLinearBlock> c_linear_block;
        res = GetC2ConstLinearBlock(buf_pack, &c_linear_block);
        if(C2_OK != res) break;

        std::unique_ptr<C2ReadView> read_view;
        res = MapConstLinearBlock(*c_linear_block, timeout, &read_view);
        if(C2_OK != res) break;

        MFX_DEBUG_TRACE_I64(buf_pack.ordinal.timestamp.peeku());

        data = read_view->data() + c_linear_block->offset();
        filled_len = c_linear_block->size();

        MFX_DEBUG_TRACE_STREAM("data: " << FormatHex(data, filled_len));

        m_frameConstructor->SetEosMode(buf_pack.flags & C2FrameData::FLAG_END_OF_STREAM);

        mfxStatus mfx_res;
        HUCVideoBuffer *hucBuffer = NULL;
        hucBuffer = (HUCVideoBuffer *) data;
        if (hucBuffer->pr_magic == PROTECTED_DATA_BUFFER_MAGIC) {
            mfx_res = m_frameConstructor->Load_data(data,
                                                    filled_len,
                                                    infobuffer,
                                                    buf_pack.ordinal.timestamp.peeku(), // pass pts
                                                    buf_pack.flags & C2FrameData::FLAG_CODEC_CONFIG,
                                                    true);
        } else {
             mfx_res = m_frameConstructor->Load(data,
                                                filled_len,
                                                buf_pack.ordinal.timestamp.peeku(), // pass pts
                                                buf_pack.flags & C2FrameData::FLAG_CODEC_CONFIG,
                                                true);
        }
        res = MfxStatusToC2(mfx_res);
        if(C2_OK != res) break;

        *frame_view = std::move(read_view);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}
