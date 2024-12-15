// Copyright (c) 2017-2024 Intel Corporation
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

#include "mfx_secure_frame_constructor.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include <iomanip>

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_secure_frame_constructor"

MfxC2SecureFrameConstructor::MfxC2SecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
    
    m_bstEnc = std::make_shared<mfxBitstream>();

    MFX_ZERO_MEMORY((*m_bstEnc));
}

MfxC2SecureFrameConstructor::~MfxC2SecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_FREE(m_bstEnc->Data);
}

MfxC2AVCSecureFrameConstructor::MfxC2AVCSecureFrameConstructor() :
    MfxC2HEVCFrameConstructor(), MfxC2SecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2AVCSecureFrameConstructor::~MfxC2AVCSecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
}

mfxStatus MfxC2SecureFrameConstructor::Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame)
{
    MFX_DEBUG_TRACE_FUNC;
    (void)pts;
    (void)header;

    mfxStatus mfx_res = MFX_ERR_NONE;

    if (!data || !size)
    {
        MFX_DEBUG_TRACE_P(data);
        MFX_DEBUG_TRACE_I32(size);
        mfx_res = MFX_ERR_NULL_PTR;
    }

    if (!complete_frame)
        mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;

    if (MFX_ERR_NONE == mfx_res) {
        HUCVideoBuffer *hucBuffer = NULL;
        hucBuffer = (HUCVideoBuffer *) data;

        if (!hucBuffer)
        {
            MFX_DEBUG_TRACE_P(hucBuffer);
            mfx_res = MFX_ERR_NULL_PTR;
        }
        else m_hucBuffer = hucBuffer;
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2AVCSecureFrameConstructor::Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool b_header, bool bCompleteFrame)
{
    MFX_DEBUG_TRACE_FUNC;

    return MfxC2FrameConstructor::Load(data, size, pts, b_header, bCompleteFrame);
}

mfxStatus MfxC2AVCSecureFrameConstructor::Load_data(const mfxU8* data, mfxU32 size, const mfxU8* bs, mfxU64 pts, bool b_header, bool bCompleteFrame)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MfxC2SecureFrameConstructor::Load(data, size, pts, b_header, bCompleteFrame);

    // if (MFX_ERR_NONE == mfx_res)
    // {
    //     MFX_DEBUG_TRACE_I32(pts);
    //     MFX_DEBUG_TRACE_P(bs);
    //     MFX_DEBUG_TRACE_I32(m_hucBuffer->pr_magic);
    //     MFX_DEBUG_TRACE_I32(m_hucBuffer->app_id);
    //     MFX_DEBUG_TRACE_I32(m_hucBuffer->session_id);
    //     MFX_DEBUG_TRACE_I32(m_hucBuffer->num_packet_data);
    //     MFX_DEBUG_TRACE_I32(m_hucBuffer->sample_size);
    //     MFX_DEBUG_TRACE_I32(m_hucBuffer->cipher_mode);
    //     std::ostringstream oss_id;
    //     oss_id << "m_hucBuffer->hw_key_data: ";
    //     for (auto byte : m_hucBuffer->hw_key_data) {
    //         oss_id << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(byte) << " ";
    //     }
    //     MFX_DEBUG_TRACE_STREAM(oss_id.str().c_str());

    //     for (int i=0; i<m_hucBuffer->num_packet_data; i++)
    //     {
    //         char* baseAddress = reinterpret_cast<char*>(m_hucBuffer);
    //         packet_info* packet = reinterpret_cast<packet_info*>(baseAddress + sizeof(HUCVideoBuffer) - 8 + (i * sizeof(packet_info)));
    //         MFX_DEBUG_TRACE_I32(packet->block_offset);
    //         MFX_DEBUG_TRACE_I32(packet->data_length);
    //         MFX_DEBUG_TRACE_I32(packet->clear_bytes);
    //         MFX_DEBUG_TRACE_I32(packet->encrypted_bytes);
    //         MFX_DEBUG_TRACE_I32(packet->pattern_clear);
    //         MFX_DEBUG_TRACE_I32(packet->pattern_encrypted);

    //         std::ostringstream oss;
    //         oss << "packet->current_iv: ";
    //         for (auto byte : packet->current_iv) {
    //             oss << std::setw(2) << std::setfill('0') << std::hex << std::uppercase << static_cast<int>(byte) << " ";
    //         }
    //         MFX_DEBUG_TRACE_STREAM(oss.str());
    //     }
    // }

    bool bFoundSps = false;
    bool bFoundPps = false;
    bool bFoundIDR = false;
    bool bFoundRegularSlice = false;
    char* baseAddress = reinterpret_cast<char*>(m_hucBuffer);

    // Save SPS/PPS if exists
    if (MFX_ERR_NONE == mfx_res)
    {
        for(int i = 0; i < m_hucBuffer->num_packet_data; i++)
        {
            data = NULL;
            size = 0;
            packet_info* packet = reinterpret_cast<packet_info*>(baseAddress + sizeof(HUCVideoBuffer) - 8 + (i * sizeof(packet_info)));

            if (packet->clear_bytes != 0)
            {
                data = bs + packet->block_offset;
                size = packet->clear_bytes;
            }
            else
            {
                continue; // All start codes are located in clear packeds, so we don't need to check encrypted packets
            }
            StartCode startCode;
            mfxU32 length;
            for (; size > 3;)
            {
                startCode = ReadStartCode(&data, &size);
                if (isSPS(startCode.type))
                {
                    auto sps = std::make_shared<mfxBitstream>();
                    sps->Data = const_cast<mfxU8*>(data) - startCode.size;

                    length = size + startCode.type;
                    startCode = ReadStartCode(&data, &size);
                    if (-1 != startCode.type)
                        length -= size + startCode.size;
                    sps->DataLength = length;
                    MFX_DEBUG_TRACE_MSG("Found SPS, length =");
                    MFX_DEBUG_TRACE_I32(length);
                    mfx_res = SaveHeaders(sps, NULL, false);
                    if (MFX_ERR_NONE != mfx_res) return mfx_res;
                    bFoundSps = true;
                }
                if (isPPS(startCode.type))
                {
                    auto pps = std::make_shared<mfxBitstream>();
                    pps->Data = const_cast<mfxU8*>(data) - startCode.size;

                    length = size + startCode.size;
                    startCode = ReadStartCode(&data, &size);
                    if (-1 != startCode.type)
                        length -= size + startCode.size;
                    pps->DataLength = length;
                    MFX_DEBUG_TRACE_MSG("Found PPS, length =");
                    MFX_DEBUG_TRACE_I32(length);
                    mfx_res = SaveHeaders(NULL, pps, false);
                    if (MFX_ERR_NONE != mfx_res) return mfx_res;
                    bFoundPps = true;
                }
                if (isIDR(startCode.type))
                {
                    MFX_DEBUG_TRACE_MSG("Found IDR");
                    bFoundIDR = true;
                    break;
                }
                if (isRegularSlice(startCode.type))
                {
                    MFX_DEBUG_TRACE_MSG("Found regular slice");
                    bFoundRegularSlice = true;
                    break;
                }
                if (-1 == startCode.type) break;
            }
        }
    }

    // alloc enough space for m_bstEnc->Data
    if (m_bstEnc->MaxLength < m_hucBuffer->sample_size)
    {
        m_bstEnc->Data = (mfxU8*)realloc(m_bstEnc->Data, m_hucBuffer->sample_size);
        if (!m_bstEnc->Data)
            return MFX_ERR_MEMORY_ALLOC;
        m_bstEnc->MaxLength = m_hucBuffer->sample_size;
    }

    packet_info* packet = reinterpret_cast<packet_info*>(baseAddress + sizeof(HUCVideoBuffer) - 8);

    // copy  data to m_bstEnc->Data
    m_bstEnc->DataOffset = 0;
    std::copy(bs, bs + m_hucBuffer->sample_size, m_bstEnc->Data);
    m_bstEnc->DataLength = m_hucBuffer->sample_size;

    // m_bstEnc->EncryptedData->Data points to encrypted part
    if (bFoundIDR || bFoundRegularSlice) {
        mfxEncryptedData *pEncryptedData = new mfxEncryptedData;
        if (pEncryptedData)
        {
            pEncryptedData->Data = m_bstEnc->Data + packet->clear_bytes;
            pEncryptedData->DataLength = packet->encrypted_bytes;
            pEncryptedData->DataOffset = 0;
            pEncryptedData->Next = NULL;
        }
        m_bstEnc->EncryptedData = pEncryptedData;
    }
    m_bstEnc->TimeStamp = pts;
    
    MFX_DEBUG_TRACE_P(m_bstEnc->Data);
    MFX_DEBUG_TRACE_P(m_bstEnc->EncryptedData->Data);

    return mfx_res;
}

IMfxC2FrameConstructor::StartCode MfxC2AVCSecureFrameConstructor::ReadStartCode(const mfxU8** position, mfxU32* size_left)
{
    MFX_DEBUG_TRACE_FUNC;

    return MfxC2AVCFrameConstructor::ReadStartCode(position, size_left);
}

std::shared_ptr<mfxBitstream> MfxC2AVCSecureFrameConstructor::GetMfxBitstream()
{
    MFX_DEBUG_TRACE_FUNC;

    auto pBitstream = MfxC2FrameConstructor::GetMfxBitstream();

    if (m_hucBuffer)
    {
        MFX_ZERO_MEMORY(m_decryptParams);
        m_decryptParams.Header.BufferId = MFX_EXTBUFF_ENCRYPTION_PARAM;
        m_decryptParams.Header.BufferSz = sizeof(mfxExtEncryptionParam);
        m_decryptParams.session = m_hucBuffer->session_id;
        m_decryptParams.uiNumSegments = m_hucBuffer->num_packet_data;
        if (m_hucBuffer->cipher_mode == OEMCrypto_CipherMode_CTR) {
            m_decryptParams.encryption_type = VA_ENCRYPTION_TYPE_SUBSAMPLE_CTR;
        } else {
            m_decryptParams.encryption_type = VA_ENCRYPTION_TYPE_SUBSAMPLE_CBC;
        }
        std::memcpy(m_decryptParams.key_blob, m_hucBuffer->hw_key_data, sizeof(m_hucBuffer->hw_key_data));

        m_decryptParams.pSegmentInfo = (EncryptionSegmentInfo*)malloc(m_hucBuffer->num_packet_data * sizeof(EncryptionSegmentInfo));
        char* baseAddress = reinterpret_cast<char*>(m_hucBuffer);
        for (int i = 0; i < m_hucBuffer->num_packet_data; i++)
        {
            packet_info* packet = reinterpret_cast<packet_info*>(baseAddress + sizeof(HUCVideoBuffer) - 8 + (i * sizeof(packet_info)));

            m_decryptParams.pSegmentInfo[i].segment_start_offset = packet->block_offset;
            m_decryptParams.pSegmentInfo[i].segment_length = packet->encrypted_bytes;
            m_decryptParams.pSegmentInfo[i].init_byte_length = packet->block_offset;
            m_decryptParams.pSegmentInfo[i].partial_aes_block_size = 0;

            IV temp_iv = packet->current_iv;
            std::memcpy(m_decryptParams.pSegmentInfo[i].aes_cbc_iv_or_ctr, temp_iv.data(), temp_iv.size());
            std::memset(m_decryptParams.pSegmentInfo[i].aes_cbc_iv_or_ctr + temp_iv.size(), 0, sizeof(m_decryptParams.pSegmentInfo[i].aes_cbc_iv_or_ctr) - temp_iv.size());
        }

        m_extBufs.clear();
        m_extBufs.push_back(reinterpret_cast<mfxExtBuffer*>(&m_decryptParams));
        m_bstEnc->ExtParam = &m_extBufs.back();
        
        m_bstEnc->NumExtParam = 1;
        m_bstEnc->DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;

        MFX_DEBUG_TRACE_I32(m_decryptParams.session);
        MFX_DEBUG_TRACE_I32(m_bstEnc->TimeStamp);
        MFX_DEBUG_TRACE_I32(m_bstEnc->DataLength);
        MFX_DEBUG_TRACE_I32(m_bstEnc->EncryptedData->DataLength);

        return m_bstEnc;
    }

    MFX_DEBUG_TRACE_P(pBitstream.get());
    return pBitstream;
}

MfxC2HEVCSecureFrameConstructor::MfxC2HEVCSecureFrameConstructor():
                                MfxC2AVCSecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2HEVCSecureFrameConstructor::~MfxC2HEVCSecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
}

std::shared_ptr<IMfxC2FrameConstructor> MfxC2SecureFrameConstructorFactory::CreateFrameConstructor(MfxC2FrameConstructorType fc_type)
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<IMfxC2FrameConstructor> fc;
    if (MfxC2FC_SEC_AVC == fc_type) {
        fc = std::make_shared<MfxC2AVCSecureFrameConstructor>();
        return fc;

    }  else if (MfxC2FC_SEC_HEVC == fc_type) { 
        fc = std::make_shared<MfxC2HEVCSecureFrameConstructor>();
        return fc;
    }

    else {
        fc = std::make_shared<MfxC2FrameConstructor>();
        return fc;
    }
}
