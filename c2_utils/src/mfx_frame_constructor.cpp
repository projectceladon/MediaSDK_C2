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

#include "mfx_frame_constructor.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_bs_utils.h"
#include "mfx_c2_hevc_bitstream.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_frame_constructor"

const std::vector<mfxU32> MfxC2HEVCFrameConstructor::NAL_UT_CODED_SLICEs = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 17, 18, 19, 20, 21 };

MfxC2FrameConstructor::MfxC2FrameConstructor():
    m_bsState(MfxC2BS_HeaderAwaiting),
    m_profile(MFX_PROFILE_UNKNOWN),
    m_bEos(false),
    m_uBstBufReallocs(0),
    m_uBstBufCopyBytes(0)
{
    MFX_DEBUG_TRACE_FUNC;

    m_bstHeader = std::make_shared<mfxBitstream>();
    m_bstBuf = std::make_shared<mfxBitstream>();
    m_bstIn = std::make_shared<mfxBitstream>();

    MFX_ZERO_MEMORY((*m_bstHeader));
    MFX_ZERO_MEMORY((*m_bstBuf));
    MFX_ZERO_MEMORY((*m_bstIn));
    MFX_ZERO_MEMORY(m_frInfo);
}

MfxC2FrameConstructor::~MfxC2FrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    if (m_bstBuf->Data) {
        MFX_DEBUG_TRACE_I32(m_bstBuf->MaxLength);
        MFX_DEBUG_TRACE_I32(m_uBstBufReallocs);
        MFX_DEBUG_TRACE_I32(m_uBstBufCopyBytes);

        MFX_FREE(m_bstBuf->Data);
    }

    MFX_FREE(m_bstHeader->Data);
}

mfxStatus MfxC2FrameConstructor::Init(
    mfxU16 profile,
    mfxFrameInfo fr_info )
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    m_profile = profile;
    m_frInfo = fr_info;
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::LoadHeader(const mfxU8* data, mfxU32 size, bool header)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(data);
    MFX_DEBUG_TRACE_I32(size);
    MFX_DEBUG_TRACE_I32(header);
    MFX_DEBUG_TRACE_I32(m_bsState);
    if (!data || !size) mfx_res = MFX_ERR_NULL_PTR;
    if (MFX_ERR_NONE == mfx_res) {
        if (header) {
            // if new header arrived after reset we are ignoring previously collected header data
            if (m_bsState == MfxC2BS_Resetting) {
                m_bsState = MfxC2BS_HeaderObtained;
            } else if (size) {
                mfxU32 needed_MaxLength = 0;
                mfxU8* new_data = nullptr;

                needed_MaxLength = m_bstHeader->DataOffset + m_bstHeader->DataLength + size; // offset should be 0
                if (m_bstHeader->MaxLength < needed_MaxLength) {
                    // increasing buffer capacity if needed
                    new_data = (mfxU8*)realloc(m_bstHeader->Data, needed_MaxLength);
                    if (new_data) {
                        // setting new values
                        m_bstHeader->Data = new_data;
                        m_bstHeader->MaxLength = needed_MaxLength;
                    }
                    else mfx_res = MFX_ERR_MEMORY_ALLOC;
                }
                if (MFX_ERR_NONE == mfx_res) {
                    mfxU8* buf = m_bstHeader->Data + m_bstHeader->DataOffset + m_bstHeader->DataLength;

                    std::copy(data, data + size, buf);
                    m_bstHeader->DataLength += size;
                }
                if (MfxC2BS_HeaderAwaiting == m_bsState) m_bsState = MfxC2BS_HeaderCollecting;
            }
        } else {
            // We have generic data. In case we are in Resetting state (i.e. seek mode)
            // we attach header to the bitstream, other wise we are moving in Obtained state.
            if (MfxC2BS_HeaderCollecting == m_bsState) {
                // As soon as we are receving first non header data we are stopping collecting header
                m_bsState = MfxC2BS_HeaderObtained;
            }
            else if (MfxC2BS_Resetting == m_bsState) {
                // if reset detected and we have header data buffered - we are going to load it
                mfx_res = BstBufRealloc(m_bstHeader->DataLength);
                if (MFX_ERR_NONE == mfx_res) {
                    mfxU8* buf = m_bstBuf->Data + m_bstBuf->DataOffset + m_bstBuf->DataLength;

                    std::copy(m_bstHeader->Data + m_bstHeader->DataOffset,
                        m_bstHeader->Data + m_bstHeader->DataOffset + m_bstHeader->DataLength, buf);
                    m_bstBuf->DataLength += m_bstHeader->DataLength;
                    m_uBstBufCopyBytes += m_bstHeader->DataLength;
                }
                m_bsState = MfxC2BS_HeaderObtained;
            }
        }
    }
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::Load_None(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    mfx_res = LoadHeader(data, size, header);
    if ((MFX_ERR_NONE == mfx_res) && m_bstBuf->DataLength) {
        mfx_res = BstBufRealloc(size);
        if (MFX_ERR_NONE == mfx_res) {
            mfxU8* buf = m_bstBuf->Data + m_bstBuf->DataOffset + m_bstBuf->DataLength;

            std::copy(data, data + size, buf);
            m_bstBuf->DataLength += size;
            m_uBstBufCopyBytes += size;
        }
    }
    if (MFX_ERR_NONE == mfx_res) {
        if (m_bstBuf->DataLength) m_bstCurrent = m_bstBuf;
        else {
            m_bstIn->Data = (mfxU8*)data;
            m_bstIn->DataOffset = 0;
            m_bstIn->DataLength = size;
            m_bstIn->MaxLength = size;
            if (complete_frame)
                m_bstIn->DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;

            m_bstCurrent = m_bstIn;
        }
        m_bstCurrent->TimeStamp = pts;
    }
    else m_bstCurrent = nullptr;
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(data);
    MFX_DEBUG_TRACE_I32(size);
    MFX_DEBUG_TRACE_I64(pts);
    if (!data || !size) mfx_res = MFX_ERR_NULL_PTR;
    if (MFX_ERR_NONE == mfx_res) {
        mfx_res = Load_None(data, size, pts, header, complete_frame);
    }
    MFX_DEBUG_TRACE__mfxBitstream((*m_bstBuf));
    MFX_DEBUG_TRACE__mfxBitstream((*m_bstIn));
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::Unload()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    mfx_res = BstBufSync();

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

// NOTE: we suppose that Load/Unload were finished
mfxStatus MfxC2FrameConstructor::Reset()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    // saving allocating information about internal buffer
    mfxU8* data = m_bstBuf->Data;
    mfxU32 allocated_length = m_bstBuf->MaxLength;

    // resetting frame constructor
    m_bstCurrent = nullptr;
    m_bstBuf = std::make_shared<mfxBitstream>();
    MFX_ZERO_MEMORY((*m_bstBuf));
    m_bstIn = std::make_shared<mfxBitstream>();
    MFX_ZERO_MEMORY((*m_bstIn));

    m_bEos = false;

    // restoring allocating information about internal buffer
    m_bstBuf->Data = data;
    m_bstBuf->MaxLength = allocated_length;

    // we have some header data and will attempt to return it
    if (m_bsState >= MfxC2BS_HeaderCollecting) m_bsState = MfxC2BS_Resetting;

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::BstBufRealloc(mfxU32 add_size)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;
    mfxU32 needed_MaxLength = 0;
    mfxU8* new_data = nullptr;

    if (add_size) {
        needed_MaxLength = m_bstBuf->DataOffset + m_bstBuf->DataLength + add_size; // offset should be 0
        if (m_bstBuf->MaxLength < needed_MaxLength) {
            // increasing buffer capacity if needed
            new_data = (mfxU8*)realloc(m_bstBuf->Data, needed_MaxLength);
            if (new_data) {
                // collecting statistics
                ++m_uBstBufReallocs;
                if (new_data != m_bstBuf->Data) m_uBstBufCopyBytes += m_bstBuf->MaxLength;
                // setting new values
                m_bstBuf->Data = new_data;
                m_bstBuf->MaxLength = needed_MaxLength;
            }
            else mfx_res = MFX_ERR_MEMORY_ALLOC;
        }
    }
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::BstBufMalloc(mfxU32 new_size)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;
    mfxU32 needed_MaxLength = 0;

    if (new_size) {
        needed_MaxLength = new_size;
        if (m_bstBuf->MaxLength < needed_MaxLength) {
            // increasing buffer capacity if needed
            MFX_FREE(m_bstBuf->Data);
            m_bstBuf->Data = (mfxU8*)malloc(needed_MaxLength);
            m_bstBuf->MaxLength = needed_MaxLength;
            ++m_uBstBufReallocs;
        }
        if (!(m_bstBuf->Data)) {
            m_bstBuf->MaxLength = 0;
            mfx_res = MFX_ERR_MEMORY_ALLOC;
        }
    }
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::BstBufSync()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    if (nullptr != m_bstCurrent) {
        if (m_bstCurrent == m_bstBuf) {
            if (m_bstBuf->DataLength && m_bstBuf->DataOffset) {
                // shifting data to the beginning of the buffer
                memmove(m_bstBuf->Data, m_bstBuf->Data + m_bstBuf->DataOffset, m_bstBuf->DataLength);
                m_uBstBufCopyBytes += m_bstBuf->DataLength;
            }
            m_bstBuf->DataOffset = 0;
        }
        if ((m_bstCurrent == m_bstIn) && m_bstIn->DataLength) {
            // copying data from m_bstIn to bst_Buf
            // Note: we read data from m_bstIn, thus here bst_Buf is empty
            mfx_res = BstBufMalloc(m_bstIn->DataLength);
            if (MFX_ERR_NONE == mfx_res) {
                std::copy(m_bstIn->Data + m_bstIn->DataOffset,
                    m_bstIn->Data + m_bstIn->DataOffset + m_bstIn->DataLength, m_bstBuf->Data);
                m_bstBuf->DataOffset = 0;
                m_bstBuf->DataLength = m_bstIn->DataLength;
                m_bstBuf->TimeStamp  = m_bstIn->TimeStamp;
                m_bstBuf->DataFlag   = m_bstIn->DataFlag;
                m_uBstBufCopyBytes += m_bstIn->DataLength;
            }
            m_bstIn = std::make_shared<mfxBitstream>();
            MFX_ZERO_MEMORY((*m_bstIn));
        }
        m_bstCurrent = nullptr;
    }
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

std::shared_ptr<mfxBitstream> MfxC2FrameConstructor::GetMfxBitstream()
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<mfxBitstream> bst;

    if (m_bstBuf->Data && m_bstBuf->DataLength) {
        bst = m_bstBuf;
    } else if (m_bstIn->Data && m_bstIn->DataLength) {
        bst = m_bstIn;
    } else {
        bst = m_bstBuf;
    }

    MFX_DEBUG_TRACE_P(m_bstIn.get());
    MFX_DEBUG_TRACE_P(m_bstBuf.get());
    MFX_DEBUG_TRACE_P(bst.get());

    return bst;
}

MfxC2AVCFrameConstructor::MfxC2AVCFrameConstructor():
    MfxC2FrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_ZERO_MEMORY(m_sps);
    MFX_ZERO_MEMORY(m_pps);
}

MfxC2AVCFrameConstructor::~MfxC2AVCFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_FREE(m_sps.Data);
    MFX_FREE(m_pps.Data);
}

mfxStatus MfxC2AVCFrameConstructor::SaveHeaders(std::shared_ptr<mfxBitstream> sps, std::shared_ptr<mfxBitstream> pps, bool is_reset)
{
    MFX_DEBUG_TRACE_FUNC;

    if (is_reset) Reset();

    if (nullptr != sps) {
        if (m_sps.MaxLength < sps->DataLength) {
            m_sps.Data = (mfxU8*)realloc(m_sps.Data, sps->DataLength);
            if (!m_sps.Data)
                return MFX_ERR_MEMORY_ALLOC;
            m_sps.MaxLength = sps->DataLength;
        }
        std::copy(sps->Data + sps->DataOffset,
            sps->Data + sps->DataOffset + sps->DataLength, m_sps.Data);
        m_sps.DataLength = sps->DataLength;
    }
    if (nullptr != pps) {
        if (m_pps.MaxLength < pps->DataLength) {
            m_pps.Data = (mfxU8*)realloc(m_pps.Data, pps->DataLength);
            if (!m_pps.Data)
                return MFX_ERR_MEMORY_ALLOC;
            m_pps.MaxLength = pps->DataLength;
        }
        std::copy(pps->Data + pps->DataOffset, pps->Data + pps->DataOffset + pps->DataLength, m_pps.Data);
        m_pps.DataLength = pps->DataLength;
    }
    return MFX_ERR_NONE;
}

mfxStatus MfxC2AVCFrameConstructor::FindHeaders(const mfxU8* data, mfxU32 size, bool &found_sps, bool &found_pps, bool &found_sei)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    found_sps = false;
    found_pps = false;
    found_sei = false;

    if (data && size) {
        StartCode start_code;
        mfxU32 length;
        for (; size > 3;) {
            start_code = ReadStartCode(&data, size);
            if (isSPS(start_code.type)) {
                std::shared_ptr<mfxBitstream> sps = std::make_shared<mfxBitstream>();
                if (!sps) return MFX_ERR_MEMORY_ALLOC;

                MFX_ZERO_MEMORY((*sps));
                sps->Data = (mfxU8*)data - start_code.size;

                length = size + start_code.size;
                start_code = ReadStartCode(&data, size);
                if (-1 != start_code.type)
                    length -= size + start_code.size;
                sps->DataLength = length;
                MFX_DEBUG_TRACE_STREAM("Found SPS size " << length);
                mfx_res = SaveHeaders(sps, nullptr, false);
                if (MFX_ERR_NONE != mfx_res) return mfx_res;
                found_sps = true;
            }
            if (isPPS(start_code.type)) {
                std::shared_ptr<mfxBitstream> pps = std::make_shared<mfxBitstream>();
                if (!pps) return MFX_ERR_MEMORY_ALLOC;

                MFX_ZERO_MEMORY((*pps));
                pps->Data = (mfxU8*)data - start_code.size;

                length = size + start_code.size;
                start_code = ReadStartCode(&data, size);
                if (-1 != start_code.type)
                    length -= size + start_code.size;
                pps->DataLength = length;
                MFX_DEBUG_TRACE_STREAM("Found PPS size " << length);
                mfx_res = SaveHeaders(nullptr, pps, false);
                if (MFX_ERR_NONE != mfx_res) return mfx_res;
                found_pps = true;
            }
            if (isIDR(start_code.type)) {
                MFX_DEBUG_TRACE_STREAM("Found IDR ");
            }
            while (isSEI(start_code.type))
            {
                mfxBitstream sei = {};
                MFX_ZERO_MEMORY(sei);
                sei.Data = (mfxU8*)data - start_code.size;
                sei.DataLength = size + start_code.size;
                start_code = ReadStartCode(&data, size);
                if (-1 != start_code.type)
                    sei.DataLength -= size + start_code.size;
                 MFX_DEBUG_TRACE_STREAM("Found SEI size " << sei.DataLength);
                 mfx_res = SaveSEI(&sei);
                 if (MFX_ERR_NONE != mfx_res) return mfx_res;
                 found_sei = true;
             }
            // start code == coded slice, so no need wait SEI
            if (!needWaitSEI(start_code.type)) found_sei = true;
            if (-1 == start_code.type) break;
        }
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2AVCFrameConstructor::LoadHeader(const mfxU8* data, mfxU32 size, bool header)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(data);
    MFX_DEBUG_TRACE_I32(size);
    MFX_DEBUG_TRACE_I32(header);
    MFX_DEBUG_TRACE_I32(m_bsState);

    bool bFoundSps = false;
    bool bFoundPps = false;
    bool bFoundSei = false;

    if (header && data && size) {
        if (MfxC2BS_HeaderAwaiting == m_bsState) m_bsState = MfxC2BS_HeaderCollecting;

        mfx_res = FindHeaders(data, size, bFoundSps, bFoundPps, bFoundSei);
        if (MFX_ERR_NONE == mfx_res && bFoundSps && bFoundPps)
            m_bsState = bFoundSei ? MfxC2BS_HeaderObtained : MfxC2BS_HeaderWaitSei;

    } else if (MfxC2BS_Resetting == m_bsState) {
        mfx_res = FindHeaders(data, size, bFoundSps, bFoundPps, bFoundSei);
        if (MFX_ERR_NONE == mfx_res) {
            if (!bFoundSps || !bFoundPps) {
                // In case we are in Resetting state (i.e. seek mode)
                // and bitstream has no headers, we attach header to the bitstream.
                mfx_res = BstBufRealloc(m_sps.DataLength + m_pps.DataLength);
                if (MFX_ERR_NONE == mfx_res) {
                    mfxU8* buf = m_bstBuf->Data + m_bstBuf->DataOffset + m_bstBuf->DataLength;
                    std::copy(m_sps.Data, m_sps.Data + m_sps.DataLength, buf);
                    buf += m_sps.DataLength;
                    std::copy(m_pps.Data, m_pps.Data + m_pps.DataLength, buf);

                    m_bstBuf->DataLength += m_sps.DataLength + m_pps.DataLength;
                    m_uBstBufCopyBytes += m_sps.DataLength + m_pps.DataLength;
                }
            }
            m_bsState = MfxC2BS_HeaderObtained;
        }
    } else if (MfxC2BS_HeaderCollecting == m_bsState) {
        // As soon as we are receving first non header data we are stopping collecting header
        m_bsState = MfxC2BS_HeaderObtained;
    } else if (MfxC2BS_HeaderWaitSei == m_bsState) {
        mfx_res = FindHeaders(data, size, bFoundSps, bFoundPps, bFoundSei);
        if (MFX_ERR_NONE == mfx_res && bFoundSps && bFoundPps)
        {
            m_bsState = bFoundSei ? MfxC2BS_HeaderObtained : MfxC2BS_HeaderWaitSei;
        }
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

IMfxC2FrameConstructor::StartCode MfxC2AVCFrameConstructor::ReadStartCode(const mfxU8** position, mfxU32& size_left)
{
    MFX_DEBUG_TRACE_FUNC;

    StartCode start_code = { .type=-1, .size=0 };
    mfxU32 zero_count = 0;
    static const mfxU8 nal_unit_type_bits = 0x1f;

    mfxI32 i = 0;
    for (; i < (mfxI32)size_left - 2; ) {
        if ((*position)[1]) {
            *position += 2;
            i += 2;
            continue;
        }

        zero_count = 0;
        if (!(*position)[0]) zero_count++;

        mfxU32 j;
        for (j = 1; j < (mfxU32)size_left - i; j++) {
            if ((*position)[j]) break;
        }

        zero_count = zero_count ? j: j - 1;

        *position += j;
        i += j;

        if (i >= (mfxI32)size_left) break;

        if (zero_count >= 2 && (*position)[0] == 1) {
            start_code.size = MFX_MIN(zero_count + 1, 4);
            size_left -= i + 1;
            (*position)++; // remove 0x01 symbol
            if (size_left >= 1) {
                start_code.type = (*position)[0] & nal_unit_type_bits;
            } else {
                *position -= start_code.size;
                size_left += start_code.size;
                start_code.size = 0;
            }
            return start_code;
        }
        zero_count = 0;
    }

    if (!zero_count) {
        for (mfxU32 k = 0; k < size_left - i; k++, (*position)++) {
            if ((*position)[0]) {
                zero_count = 0;
                continue;
            }
            zero_count++;
        }
    }

    zero_count = MFX_MIN(zero_count, 3);
    *position -= zero_count;
    size_left = zero_count;
    return start_code;
}

mfxStatus MfxC2AVCFrameConstructor::Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    mfx_res = MfxC2FrameConstructor::Load(data, size, pts, header, complete_frame);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

MfxC2HEVCFrameConstructor::MfxC2HEVCFrameConstructor():
    MfxC2AVCFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2HEVCFrameConstructor::~MfxC2HEVCFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
}

IMfxC2FrameConstructor::StartCode MfxC2HEVCFrameConstructor::ReadStartCode(const mfxU8** position, mfxU32& size_left)
{
    MFX_DEBUG_TRACE_FUNC;

    StartCode start_code = { .type=-1, .size=0 };
    mfxU32 zero_count = 0;
    static const mfxU8 NAL_UNITTYPE_BITS_H265 = 0x7e;
    static const mfxU8 NAL_UNITTYPE_SHIFT_H265 = 1;

    mfxI32 i = 0;
    for (; i < (mfxI32)size_left - 2; ) {
        if ((*position)[1]) {
            *position += 2;
            i += 2;
            continue;
        }

        zero_count = 0;
        if (!(*position)[0]) zero_count++;

        mfxU32 j;
        for (j = 1; j < (mfxU32)size_left - i; j++) {
            if ((*position)[j]) break;
        }

        zero_count = zero_count ? j: j - 1;

        *position += j;
        i += j;

        if (i >= (mfxI32)size_left) break;

        if (zero_count >= 2 && (*position)[0] == 1) {
            start_code.size = MFX_MIN(zero_count + 1, 4);
            size_left -= i + 1;
            (*position)++; // remove 0x01 symbol
            if (size_left >= 1) {
                start_code.type = ((*position)[0] & NAL_UNITTYPE_BITS_H265) >> NAL_UNITTYPE_SHIFT_H265;
            } else {
                *position -= start_code.size;
                size_left += start_code.size;
                start_code.size = 0;
            }
            return start_code;
        }
        zero_count = 0;
    }

    if (!zero_count) {
        for (mfxU32 k = 0; k < size_left - i; k++, (*position)++) {
            if ((*position)[0]) {
                zero_count = 0;
                continue;
            }
            zero_count++;
        }
    }

    zero_count = MFX_MIN(zero_count, 3);
    *position -= zero_count;
    size_left = zero_count;
    return start_code;
}

mfxStatus MfxC2HEVCFrameConstructor::SaveSEI(mfxBitstream *pSEI)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    if (nullptr != pSEI && nullptr != pSEI->Data)
    {
        std::vector<mfxU8> swappingMemory;
        mfxU32 swappingMemorySize = pSEI->DataLength - 5;
        swappingMemory.resize(swappingMemorySize + 8);

        std::vector<mfxU32> SEINames = {SEI_MASTERING_DISPLAY_COLOUR_VOLUME, SEI_CONTENT_LIGHT_LEVEL_INFO};
        for (auto const& sei_name : SEINames) // look for sei
        {
            mfxPayload sei = {};
            sei.BufSize = pSEI->DataLength;
            sei.Data = (mfxU8*)realloc(sei.Data, pSEI->DataLength);
            if (nullptr == sei.Data)
            {
                MFX_DEBUG_TRACE_MSG("ERROR: SEI was not alloacated");
                return MFX_ERR_MEMORY_ALLOC;
            }

            MFX_DEBUG_TRACE_MSG("Calling ByteSwapper::SwapMemory()");

            BytesSwapper::SwapMemory(&(swappingMemory[0]), swappingMemorySize, (pSEI->Data + 5), swappingMemorySize);

            MFX_DEBUG_TRACE_MSG("Calling HEVCHeadersBitstream.Reset()");
            MFX_DEBUG_TRACE_U32(swappingMemorySize);

            HEVCParser::HEVCHeadersBitstream bitStream;
            bitStream.Reset(&(swappingMemory[0]), swappingMemorySize);

            MFX_DEBUG_TRACE_MSG("Calling HEVCHeadersBitstream.GetSEI() for SEI");
            MFX_DEBUG_TRACE_U32(sei_name);

            MFX_TRY_AND_CATCH(
                bitStream.GetSEI(&sei, sei_name),
                sei.NumBit = 0);
            if (sei.Type == sei_name && sei.NumBit > 0)
            {
                // replace sei
                auto old_sei = m_SEIMap.find(sei_name);
                if (old_sei != m_SEIMap.end())
                {
                    MFX_FREE(old_sei->second.Data);
                    m_SEIMap.erase(old_sei);
                }
                m_SEIMap.insert(std::pair<mfxU32, mfxPayload>(sei_name, sei));
            }
            else
                MFX_FREE(sei.Data);
        }
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxPayload* MfxC2HEVCFrameConstructor::GetSEI(mfxU32 type)
{
    auto sei = m_SEIMap.find(type);
    if (sei != m_SEIMap.end())
        return &(sei->second);

    return nullptr;
}

std::shared_ptr<IMfxC2FrameConstructor> MfxC2FrameConstructorFactory::CreateFrameConstructor(MfxC2FrameConstructorType fc_type)
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<IMfxC2FrameConstructor> fc;

    switch (fc_type)
    {
    case MfxC2FC_AVC:
        fc = std::make_shared<MfxC2AVCFrameConstructor>();
        break;
    case MfxC2FC_HEVC:
        fc = std::make_shared<MfxC2HEVCFrameConstructor>();
        break;
#ifdef ENABLE_WIDEVINE
    case MfxC2FC_SEC_AVC:
        fc = std::make_shared<MfxC2AVCSecureFrameConstructor>();
        break;
    case MfxC2FC_SEC_HEVC:
        fc = std::make_shared<MfxC2HEVCSecureFrameConstructor>();
        break;
#endif 

    default:
        break;
    }

    return fc;
}

#ifdef ENABLE_WIDEVINE

MfxC2SecureFrameConstructor::MfxC2SecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_ZERO_MEMORY(m_hucBuffer);

    MFX_ZERO_MEMORY(m_SPS_PPS_SEI);
    MFX_ZERO_MEMORY(m_ClearBst);

    m_sliceHeader.resize(SLICE_HEADER_BUFFER_SIZE);
}

MfxC2SecureFrameConstructor::~MfxC2SecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;
}

mfxStatus MfxC2SecureFrameConstructor::Reset(void)
{
    MFX_DEBUG_TRACE_FUNC;
    for (std::list<mfxEncryptedData *>::iterator it = m_encryptedDataList.begin(); it != m_encryptedDataList.end(); ++it)
    {
        (*it)->DataLength = 0;
    }

    MFX_ZERO_MEMORY(m_hucBuffer);

    ResetHeaders();

    return MFX_ERR_NONE;
}

mfxStatus MfxC2SecureFrameConstructor::ResetHeaders(void)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    memset(m_SPS_PPS_SEI.Data, 0, m_SPS_PPS_SEI.DataLength);
    m_SPS_PPS_SEI.DataLength = 0;

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2SecureFrameConstructor::ConstructFrame(VACencStatusBuf* cencStatus, mfxBitstream* bs)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    m_ClearBst.DataLength = 0;
    m_ClearBst.DataOffset = 0;

    bool updateHeaders = !(m_SPS_PPS_SEI.DataLength == cencStatus->buf_size &&
                           m_SPS_PPS_SEI.Data &&
                           !memcmp(m_SPS_PPS_SEI.Data, cencStatus->buf, m_SPS_PPS_SEI.DataLength));

    // Update saved SPS/PPS/SEI headers and attach to m_ClearBst when the new headers come
    if (updateHeaders)
    {
        if (m_SPS_PPS_SEI.MaxLength < cencStatus->buf_size)
        {
            m_SPS_PPS_SEI.Data = (mfxU8*)realloc(m_SPS_PPS_SEI.Data, cencStatus->buf_size);
            if (m_SPS_PPS_SEI.Data)
                m_SPS_PPS_SEI.MaxLength = cencStatus->buf_size;
            else
                mfx_res = MFX_ERR_MEMORY_ALLOC;
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            std::copy((mfxU8*)cencStatus->buf, (mfxU8*)cencStatus->buf + cencStatus->buf_size, m_SPS_PPS_SEI.Data);
            m_SPS_PPS_SEI.DataLength = cencStatus->buf_size;

            mfx_res = ParseHeaders();
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            if (m_ClearBst.MaxLength < m_SPS_PPS_SEI.DataLength)
            {
                m_ClearBst.Data = (mfxU8*)realloc(m_ClearBst.Data, m_SPS_PPS_SEI.DataLength);
                if (m_ClearBst.Data)
                    m_ClearBst.MaxLength = m_SPS_PPS_SEI.DataLength;
                else
                    mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            std::copy(m_SPS_PPS_SEI.Data, m_SPS_PPS_SEI.Data + m_SPS_PPS_SEI.DataLength, m_ClearBst.Data);
            m_ClearBst.DataLength = m_SPS_PPS_SEI.DataLength;
        }
    }

    // Reconstruct Slice Header as a bitstream and attach to m_ClearBst
    if (MFX_ERR_NONE == mfx_res)
    {
        if (cencStatus->slice_buf_type != VACencSliceBufType::VaCencSliceBufParamter)
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;

        mfxU8* pPackedSliceHdr = NULL;
        mfxU32 packedSliceHdrLength = 0;
        if (MFX_ERR_NONE == mfx_res)
        {
            mfx_res = GetSliceHeader((mfxU8*)cencStatus->slice_buf,
                                     cencStatus->slice_buf_size,
                                     &pPackedSliceHdr,
                                     packedSliceHdrLength);
        }
        if (MFX_ERR_NONE == mfx_res && (!pPackedSliceHdr || !packedSliceHdrLength))
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;

        if (MFX_ERR_NONE == mfx_res)
        {
            mfxU32 neededMaxLength = m_ClearBst.DataLength + packedSliceHdrLength;
            if (m_ClearBst.MaxLength < neededMaxLength)
            {
                m_ClearBst.Data = (mfxU8*)realloc(m_ClearBst.Data, neededMaxLength);
                if (m_ClearBst.Data)
                    m_ClearBst.MaxLength = neededMaxLength;
                else
                    mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        }
        if (MFX_ERR_NONE == mfx_res)
        {
            mfxU8* endOfData = m_ClearBst.Data + m_ClearBst.DataLength;
            std::copy(pPackedSliceHdr, pPackedSliceHdr + packedSliceHdrLength, endOfData);
            m_ClearBst.DataLength += packedSliceHdrLength;
        }
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        MFX_DEBUG_TRACE__mfxBitstream(m_ClearBst);
        *bs = m_ClearBst;
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
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

    C2SecureBuffer *secureBuffer = NULL;
    if (MFX_ERR_NONE == mfx_res)
    {
        ProtectedBufferHandle *pbh = (ProtectedBufferHandle *) data;
        MFX_DEBUG_TRACE_P(pbh);
        if (pbh == NULL || !pbh->isGoodMagic())
        {
            MFX_DEBUG_TRACE_MSG("Wrong magic (Protected Buffer Handle) value");
            mfx_res = MFX_ERR_NULL_PTR;
        }
        else
        {
            secureBuffer = (C2SecureBuffer *) pbh->getC2Buf();
            MFX_DEBUG_TRACE_P(secureBuffer);
            if (!secureBuffer)
            {
                MFX_DEBUG_TRACE_MSG("secure buffer handle is NULL");
                mfx_res = MFX_ERR_NULL_PTR;
            }
        }
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        HUCVideoBuffer *hucBuffer = NULL;
        hucBuffer = &(secureBuffer->hucBuffer);
        if (!hucBuffer)
        {
            MFX_DEBUG_TRACE_P(hucBuffer);
            mfx_res = MFX_ERR_NULL_PTR;
        }
        else m_hucBuffer = *hucBuffer;
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxEncryptedData* MfxC2SecureFrameConstructor::GetFreeEncryptedDataItem(void)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxEncryptedData* pEncryptedData = NULL;
    for (std::list<mfxEncryptedData *>::iterator it = m_encryptedDataList.begin(); it != m_encryptedDataList.end(); ++it)
    {
        if (0 == (*it)->DataLength)
        {
            MFX_DEBUG_TRACE_MSG("Found free mfxEncryptedData item");
            pEncryptedData = *it;
            m_encryptedDataList.splice(m_encryptedDataList.end(), m_encryptedDataList, it); // move an item to the end of the list
            break;
        }
    }
    if (NULL == pEncryptedData)
    {
        pEncryptedData = (mfxEncryptedData*) calloc(1, sizeof(mfxEncryptedData));
        if (pEncryptedData)
        {
            m_encryptedDataList.push_back(pEncryptedData);
            MFX_DEBUG_TRACE_MSG("Created new mfxEncryptedData item");
        }
    }

    MFX_DEBUG_TRACE_P(pEncryptedData);
    return pEncryptedData;
}

mfxEncryptedData* MfxC2SecureFrameConstructor::BuildEncryptedDataList(void)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxEncryptedData* first = NULL;
    std::list<mfxEncryptedData *>::iterator it;
    for (it = m_encryptedDataList.begin(); it != m_encryptedDataList.end(); ++it)
    {
        if ((*it)->DataLength)
        {
            first = *it;
            break;
        }
    }
    if (it != m_encryptedDataList.end())
    {
        std::list<mfxEncryptedData *>::iterator next = it;
        next++;
        for (; next != m_encryptedDataList.end(); ++it, ++next)
        {
            if ((*next)->DataLength)
            {
                (*it)->Next = *next;
            }
        }
    }

    MFX_DEBUG_TRACE_P(first);
    return first;
}

mfxStatus MfxC2SecureFrameConstructor::GetSliceHeader(mfxU8* data, mfxU32 size, mfxU8 **sliceHdr, mfxU32 &sliceHdrlength)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    memset(&m_sliceHeader[0], 0, sizeof(mfxU8) * m_sliceHeader.size());
    mfxU8 *sliceBufferBegin = &(*m_sliceHeader.begin());
    mfxU8 *sliceBufferEnd   = &(*m_sliceHeader.end());

    OutputBitstream obs(sliceBufferBegin, sliceBufferEnd, false);

    if (MFX_ERR_NONE == mfx_res)
    {
        mfx_res = PackSliceHeader(obs, data, size);
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        if (sliceHdr)
            *sliceHdr = sliceBufferBegin;
        sliceHdrlength = (obs.GetNumBits() + 7) / 8;
    }

    MFX_DEBUG_TRACE_P(*sliceHdr);
    MFX_DEBUG_TRACE_I32(sliceHdrlength);
    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

MfxC2AVCSecureFrameConstructor::MfxC2AVCSecureFrameConstructor() :
    MfxC2HEVCFrameConstructor(), MfxC2SecureFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    m_bNeedAttachSPSPPS = false;
}

MfxC2AVCSecureFrameConstructor::~MfxC2AVCSecureFrameConstructor(void)
{
    MFX_DEBUG_TRACE_FUNC;
}

mfxStatus MfxC2AVCSecureFrameConstructor::Reset(void)
{
    MFX_DEBUG_TRACE_FUNC;

    m_bNeedAttachSPSPPS = false;

    mfxStatus mfx_res = MfxC2SecureFrameConstructor::Reset();

    return (MFX_ERR_NONE == mfx_res) ? MfxC2FrameConstructor::Reset() : mfx_res;
}

mfxStatus MfxC2AVCSecureFrameConstructor::Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool b_header, bool bCompleteFrame)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MfxC2SecureFrameConstructor::Load(data, size, pts, b_header, bCompleteFrame);

    bool bFoundSps = false;
    bool bFoundPps = false;
    bool bFoundIDR = false;
    bool bFoundRegularSlice = false;

    // Save SPS/PPS if exists
    if (MFX_ERR_NONE == mfx_res)
    {
        for(int i = 0; i < m_hucBuffer.uiNumPackets;i++)
        {
            data = NULL;
            size = 0;
            if (m_hucBuffer.sPacketData[i].clear)
            {
                data = m_hucBuffer.base + m_hucBuffer.sPacketData[i].clearPacketOffset;
                size = m_hucBuffer.sPacketData[i].clearPacketSize;
            }
            else
            {
                continue; // All start codes are located in clear packeds, so we don't need to check encrypted packets
            }
            StartCode startCode;
            mfxU32 length;
            for (; size > 3;)
            {
                startCode = ReadStartCode(&data, size);
                if (isSPS(startCode.type))
                {
                    auto sps = std::make_shared<mfxBitstream>();
                    sps->Data = const_cast<mfxU8*>(data) - startCode.size;

                    length = size + startCode.type;
                    startCode = ReadStartCode(&data, size);
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
                    startCode = ReadStartCode(&data, size);
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

    if (MFX_ERR_NONE == mfx_res)
    {
        // Handle IDR or regular frame. Otherwise skip the buffer
        if (bFoundIDR || bFoundRegularSlice)
        {
            mfxU32 srcOffset = 0;
            if (m_hucBuffer.sPacketData[0].clear)
                srcOffset = m_hucBuffer.sPacketData[0].clearPacketOffset;
            else
                srcOffset = m_hucBuffer.sPacketData[0].sSegmentData.uiSegmentStartOffset;

            // Add new packed with SPS/PPS at the beginning of the list if it's needed
            if (m_bNeedAttachSPSPPS && (!bFoundSps || !bFoundPps))
            {
                if (m_hucBuffer.uiNumPackets >= MAX_SUPPORTED_PACKETS)
                    mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
                else
                {
                    MFX_DEBUG_TRACE_MSG("Modify m_hucBuffer to add SPS and/or PPS");

                    // Create and fill new clear packet
                    packet_info newPacket;
                    MFX_ZERO_MEMORY(newPacket);
                    newPacket.clear = 1;
                    if (!bFoundSps)
                        newPacket.clearPacketSize += m_sps.DataLength;
                    if (!bFoundPps)
                        newPacket.clearPacketSize += m_pps.DataLength;

                    for (int i=m_hucBuffer.uiNumPackets; i>0; i--)
                    {
                        m_hucBuffer.sPacketData[i] = m_hucBuffer.sPacketData[i-1];
                        if (m_hucBuffer.sPacketData[i].clear)
                        {
                            m_hucBuffer.sPacketData[i].clearPacketOffset -= srcOffset;
                            m_hucBuffer.sPacketData[i].clearPacketOffset += newPacket.clearPacketSize;
                        }
                        else
                        {
                            m_hucBuffer.sPacketData[i].sSegmentData.uiSegmentStartOffset -= srcOffset;
                            m_hucBuffer.sPacketData[i].sSegmentData.uiSegmentStartOffset += newPacket.clearPacketSize;
                        }
                    }

                    m_hucBuffer.sPacketData[0] = newPacket;
                    m_hucBuffer.uiNumPackets++;
                    m_hucBuffer.frame_size += m_hucBuffer.sPacketData[0].clearPacketSize;
                }
            }

            // Handle hucBuffer
            if (MFX_ERR_NONE == mfx_res)
            {
                if (m_bstBuf->DataLength)
                {
                    mfx_res = BstBufRealloc(sizeof(HUCVideoBuffer));
                    if (MFX_ERR_NONE == mfx_res)
                    {
                        uint8_t *src = reinterpret_cast<uint8_t*>(&m_hucBuffer);
                        std::copy(src, src + sizeof(HUCVideoBuffer), m_bstBuf->Data + m_bstBuf->DataOffset + m_bstBuf->DataLength);
                        m_bstBuf->DataLength += sizeof(HUCVideoBuffer);
                        m_uBstBufCopyBytes += sizeof(HUCVideoBuffer);
                    }
                }

                if (MFX_ERR_NONE == mfx_res)
                {
                    if (m_bstBuf->DataLength) m_bstCurrent = m_bstBuf;
                    else
                    {
                        m_bstIn->Data = (mfxU8*)&m_hucBuffer;
                        m_bstIn->DataOffset = 0;
                        m_bstIn->DataLength = sizeof(HUCVideoBuffer);
                        m_bstIn->MaxLength = sizeof(HUCVideoBuffer);
                        m_bstIn->DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;
                        m_bstCurrent = m_bstIn;
                    }
                    m_bstCurrent->TimeStamp = pts;
                }
                else m_bstCurrent = NULL;
            }

            // Handle bitstream
            mfxEncryptedData *pEncryptedData = NULL;
            if (MFX_ERR_NONE == mfx_res)
            {
                pEncryptedData = GetFreeEncryptedDataItem();
                if (!pEncryptedData)
                    mfx_res = MFX_ERR_MEMORY_ALLOC;

                if (MFX_ERR_NONE == mfx_res)
                {
                    if (pEncryptedData->MaxLength < m_hucBuffer.frame_size)
                    {
                        pEncryptedData->Data = (mfxU8*)realloc(pEncryptedData->Data, m_hucBuffer.frame_size);
                        if (pEncryptedData->Data)
                            pEncryptedData->MaxLength = m_hucBuffer.frame_size;
                        else
                            mfx_res = MFX_ERR_MEMORY_ALLOC;
                    }
                }

                if (MFX_ERR_NONE == mfx_res)
                {
                    pEncryptedData->Next = NULL;

                    mfxU32 dstOffset = 0;
                    if (m_bNeedAttachSPSPPS && !bFoundSps)
                    {
                        std::copy(m_sps.Data, m_sps.Data + m_sps.DataLength, pEncryptedData->Data + dstOffset);
                        dstOffset += m_sps.DataLength;
                    }
                    if (m_bNeedAttachSPSPPS && !bFoundPps)
                    {
                        std::copy(m_pps.Data, m_pps.Data + m_pps.DataLength, pEncryptedData->Data + dstOffset);
                        dstOffset += m_pps.DataLength;
                    }

                    std::copy(m_hucBuffer.base + srcOffset, m_hucBuffer.base + m_hucBuffer.frame_size - dstOffset, pEncryptedData->Data + dstOffset);
                    pEncryptedData->DataLength = m_hucBuffer.frame_size;
                    pEncryptedData->DataOffset = 0;
                }
            }

            /*if (MFX_ERR_NONE == mfx_res)
            {
                MFX_DEBUG_TRACE_P(m_hucBuffer.pLibInstance);
                MFX_DEBUG_TRACE_P(m_hucBuffer.base);
                MFX_DEBUG_TRACE_I32(m_hucBuffer.frame_size);
                MFX_DEBUG_TRACE_I32(m_hucBuffer.uiNumPackets);
                for (int i=0; i<m_hucBuffer.uiNumPackets; i++)
                {
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].clear);
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].clearPacketSize);
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].clearPacketOffset);
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].configData);
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].sSegmentData.uiSegmentStartOffset);
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].sSegmentData.uiSegmentLength);
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].sSegmentData.uiPartialAesBlockSizeInBytes);
                    MFX_DEBUG_TRACE_I32(m_hucBuffer.sPacketData[i].sSegmentData.uiInitByteLength);
                }
                MFX_DEBUG_TRACE_I32(m_hucBuffer.drm_type);
                MFX_DEBUG_TRACE_I32(m_hucBuffer.ucSizeOfLength);
                MFX_DEBUG_TRACE_I32(m_hucBuffer.uiCurrSegPartialAesBlockSizeInBytes);
                MFX_DEBUG_TRACE_I32(m_hucBuffer.uiEncDataSize);

                if (pEncryptedData->Data)
                {
                    char buffer[128];
                    int bstSize = pEncryptedData->DataLength > 128 ? 128 : pEncryptedData->DataLength;
                    for (int i=0; i<(bstSize+15)/16; i++)
                    {
                        memset(buffer, 0, 128);
                        int rest = bstSize - i*16;
                        for (int j=0; j<((rest<16) ? rest:16); j++)
                        {
                            mfxU8 sign = *(pEncryptedData->Data + i*16 + j);
                            sprintf(buffer + j*3, "%x%x ", sign/16, sign%16);
                        }
                        MFX_DEBUG_TRACE_MSG(buffer);
                    }
                }
            }*/

            // Handle AppId if it's required
            if (MFX_ERR_NONE == mfx_res)
            {
                MFX_DEBUG_TRACE_MSG("Copy AppId from hucBuffer");
                pEncryptedData->AppId = m_hucBuffer.appID;
                MFX_DEBUG_TRACE_I32(pEncryptedData->AppId);

                if (pEncryptedData->AppId == PAVP_APPID_INVALID)
                {
                    MFX_DEBUG_TRACE_MSG("Invalid PAVP AppID value");
                    mfx_res = MFX_WRN_OUT_OF_RANGE;
                }
            }

            m_bNeedAttachSPSPPS = false;
        }
        else
        {
            if (bFoundSps || bFoundPps)
                m_bNeedAttachSPSPPS = true;
            MFX_DEBUG_TRACE_MSG("Not enough data, skip buffer");
        }
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

std::shared_ptr<mfxBitstream> MfxC2AVCSecureFrameConstructor::GetMfxBitstream()
{
    MFX_DEBUG_TRACE_FUNC;

    auto pBitstream = MfxC2FrameConstructor::GetMfxBitstream();
    if (pBitstream)
    {
        pBitstream->EncryptedData = BuildEncryptedDataList();
        pBitstream->DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;
    }

    MFX_DEBUG_TRACE_P(pBitstream.get());
    return pBitstream;
}

mfxStatus MfxC2AVCSecureFrameConstructor::ParseHeaders()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    const mfxU8* data = m_SPS_PPS_SEI.Data;
    mfxU32 size = m_SPS_PPS_SEI.DataLength;

    StartCode startCode;
    startCode = ReadStartCode(&data, size);

    while (true)
    {
        if (isSPS(startCode.type) || isPPS(startCode.type))
        {
            mfxU8* const nalu_ptr = const_cast<mfxU8*>(data);
            mfxU32 length = size; // skip start code

            startCode = ReadStartCode(&data, size);
            MFX_DEBUG_TRACE_U32(startCode.type);
            if (-1 != startCode.type)
                length -= size + startCode.size;

            mfx_res = ParseNalUnit(nalu_ptr, length);
            if (MFX_ERR_NONE != mfx_res) break;
        }
        else
        {
            startCode = ReadStartCode(&data, size);
            MFX_DEBUG_TRACE_U32(startCode.type);
        }
        if (-1 == startCode.type) break;
        if (size <= 3) break;
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2AVCSecureFrameConstructor::ParseNalUnit(mfxU8 * const data, mfxU32 NAlUnitSize)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    using namespace AVCParser;

    mfxU32 swappingSize = NAlUnitSize;
    mfxU8 *swappingMemory = GetMemoryForSwapping(swappingSize);

    if (swappingMemory)
        BytesSwapper::SwapMemory(swappingMemory, swappingSize, data, NAlUnitSize);
    else
        mfx_res = MFX_ERR_MEMORY_ALLOC;

    if (MFX_ERR_NONE == mfx_res)
    {
        AVCHeadersBitstream bitStream;
        MFX_DEBUG_TRACE_MSG("Calling bitStream.Reset()");
        bitStream.Reset(swappingMemory, swappingSize);

        NAL_Unit_Type nalUnitType = NAL_UT_UNSPECIFIED;
        mfxU8 nalStorageIDC;

        MFX_DEBUG_TRACE_MSG("Calling bitStream.GetNALUnitType()");
        bitStream.GetNALUnitType(nalUnitType, nalStorageIDC);

        if (NAL_UT_SPS == nalUnitType)
        {
            MFX_DEBUG_TRACE_MSG("Found SPS");
            m_H264Headers.Reset();

            AVCSeqParamSet sps;
            mfx_res = bitStream.GetSequenceParamSet(&sps);
            if (MFX_ERR_NONE == mfx_res)
            {
                m_H264Headers.m_seqParams.AddHeader(&sps);
                m_H264Headers.m_seqParams.SetCurrentID(sps.GetID());
            }
            else
            {
                MFX_DEBUG_TRACE_MSG("ERROR: Invalid SPS");
                mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else if (NAL_UT_PPS == nalUnitType)
        {
            MFX_DEBUG_TRACE_MSG("Found PPS");
            AVCPicParamSet pps;
            // set illegal id
            pps.pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS;

            // Get id
            mfx_res = bitStream.GetPictureParamSetPart1(&pps);
            if (MFX_ERR_NONE == mfx_res)
            {
                AVCSeqParamSet *pRefsps = m_H264Headers.m_seqParams.GetHeader(pps.seq_parameter_set_id);
                // Get rest of pic param set
                mfx_res = bitStream.GetPictureParamSetPart2(&pps, pRefsps);
                if (MFX_ERR_NONE == mfx_res)
                {
                    m_H264Headers.m_picParams.AddHeader(&pps);
                    m_H264Headers.m_picParams.SetCurrentID(pps.GetID());
                }
                else
                {
                    MFX_DEBUG_TRACE_MSG("ERROR: Invalid PPS");
                    mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
            else
            {
                MFX_DEBUG_TRACE_MSG("ERROR: Invalid PPS");
                mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else if (NAL_UT_SEI == nalUnitType)
        {
            MFX_DEBUG_TRACE_MSG("Found SEI");
            mfx_res = MFX_ERR_NONE;
        }
        else
        {
            MFX_DEBUG_TRACE_MSG("Found unknown NAL unit");
            MFX_DEBUG_TRACE_U32(nalUnitType);
            mfx_res = MFX_ERR_UNSUPPORTED;
        }
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2AVCSecureFrameConstructor::PackSliceHeader(OutputBitstream & obs, mfxU8* data, mfxU32 size)
{
    MFX_DEBUG_TRACE_FUNC; 
    mfxStatus mfx_res = MFX_ERR_NONE;

    using namespace AVCParser;

    AVCSeqParamSet *sps = NULL;
    AVCPicParamSet *pps = NULL;
    VACencSliceParameterBufferH264 sliceParams{};

    pps = m_H264Headers.m_picParams.GetCurrentHeader();
    if (NULL == pps)
    {
        MFX_DEBUG_TRACE_MSG("ERROR: PPS not found");
        mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        sps = m_H264Headers.m_seqParams.GetCurrentHeader();
        if (NULL == sps)
        {
            MFX_DEBUG_TRACE_MSG("ERROR: SPS not found");
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        if (size != sizeof(VACencSliceParameterBufferH264))
        {
            MFX_DEBUG_TRACE_MSG("ERROR: incorrect slice_buf_size");
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        sliceParams = *(reinterpret_cast<VACencSliceParameterBufferH264*>(data));

        MFX_DEBUG_TRACE_I32(sliceParams.nal_ref_idc);
        MFX_DEBUG_TRACE_I32(sliceParams.idr_pic_flag);
        MFX_DEBUG_TRACE_I32(sliceParams.slice_type);
        MFX_DEBUG_TRACE_I32(sliceParams.field_frame_flag);
        MFX_DEBUG_TRACE_I32(sliceParams.frame_number);
        MFX_DEBUG_TRACE_I32(sliceParams.idr_pic_id);
        MFX_DEBUG_TRACE_I32(sliceParams.pic_order_cnt_lsb);
        MFX_DEBUG_TRACE_I32(sliceParams.delta_pic_order_cnt_bottom);
        MFX_DEBUG_TRACE_I32(sliceParams.delta_pic_order_cnt[0]);
        MFX_DEBUG_TRACE_I32(sliceParams.delta_pic_order_cnt[1]);
        MFX_DEBUG_TRACE_I32(sliceParams.ref_pic_fields.bits.no_output_of_prior_pics_flag);
        MFX_DEBUG_TRACE_I32(sliceParams.ref_pic_fields.bits.long_term_reference_flag);
        MFX_DEBUG_TRACE_I32(sliceParams.ref_pic_fields.bits.adaptive_ref_pic_marking_mode_flag);
        MFX_DEBUG_TRACE_I32(sliceParams.ref_pic_fields.bits.dec_ref_pic_marking_count);

        mfxU32 idrPicFlag = sliceParams.idr_pic_flag;
        mfxU32 nalRefIdc = sliceParams.nal_ref_idc;
        mfxU32 nalUnitType = sliceParams.idr_pic_flag ? NAL_UT_IDR_SLICE : NAL_UT_SLICE;
        mfxU32 fieldPicFlag = (sliceParams.field_frame_flag == VA_TOP_FIELD) || (sliceParams.field_frame_flag == VA_BOTTOM_FIELD);
        mfxU32 bottomFieldFlag = (sliceParams.field_frame_flag == VA_BOTTOM_FIELD);
        mfxU32 firstMbInSlice = 0;
        mfxU32 sliceType = sliceParams.slice_type;
        mfxU32 directSpatialMvPredFlag = 0;
        mfxU32 cabacInitIdc = 0;
        mfxU32 slice_qp_delta = 0;
        mfxU32 numRefIdxL0Active = pps->num_ref_idx_l0_active;
        mfxU32 numRefIdxL1Active = pps->num_ref_idx_l1_active;

        mfxU8 startcode[3] = { 0, 0, 1 };
        obs.PutRawBytes(startcode, startcode + sizeof startcode);
        obs.PutBit(0);
        obs.PutBits(nalRefIdc, 2);
        obs.PutBits(nalUnitType, 5);

        obs.PutUe(firstMbInSlice);
        obs.PutUe(sliceType + 5);
        obs.PutUe(pps->pic_parameter_set_id);
        obs.PutBits(sliceParams.frame_number, sps->log2_max_frame_num);
        if (!sps->frame_mbs_only_flag)
        {
            obs.PutBit(fieldPicFlag);
            if (fieldPicFlag)
                obs.PutBit(bottomFieldFlag);
        }
        if (idrPicFlag)
        {
            obs.PutUe(sliceParams.idr_pic_id);
        }
        if (sps->pic_order_cnt_type == 0)
        {
            obs.PutBits(sliceParams.pic_order_cnt_lsb, sps->log2_max_pic_order_cnt_lsb);
            if (pps->pic_order_present_flag && !fieldPicFlag)
                obs.PutSe(sliceParams.delta_pic_order_cnt_bottom);
        }
        if (sps->pic_order_cnt_type == 1 && !sps->delta_pic_order_always_zero_flag)
        {
            obs.PutSe(sliceParams.delta_pic_order_cnt[0]);
            if (pps->pic_order_present_flag && !fieldPicFlag)
                obs.PutSe(sliceParams.delta_pic_order_cnt[1]);
        }
        if (sliceType == BPREDSLICE)
        {
            obs.PutBit(directSpatialMvPredFlag);
        }
        if (sliceType != INTRASLICE)
        {
            numRefIdxL0Active = 1; //IPP_MAX(1, task.m_list0[fieldId].Size());
            numRefIdxL1Active = 1; //IPP_MAX(1, task.m_list1[fieldId].Size());
            mfxU32 numRefIdxActiveOverrideFlag =
                (numRefIdxL0Active != pps->num_ref_idx_l0_active - 1) ||
                (numRefIdxL1Active != pps->num_ref_idx_l1_active && sliceType == BPREDSLICE);

            obs.PutBit(numRefIdxActiveOverrideFlag);
            if (numRefIdxActiveOverrideFlag)
            {
                obs.PutUe(numRefIdxL0Active - 1);
                if (sliceType == BPREDSLICE)
                    obs.PutUe(numRefIdxL1Active - 1);
            }
        }
        if (sliceType != INTRASLICE)
            obs.PutBit(0); // ref_pic_list_modification_flag_l0
        if (sliceType == BPREDSLICE)
            obs.PutBit(0); // ref_pic_list_modification_flag_l1

        // prediction weight table
        if ( (pps->weighted_pred_flag &&
            ((PREDSLICE == sliceType) || (S_PREDSLICE == sliceType))) ||
            ((pps->weighted_bipred_idc == 1) && (BPREDSLICE == sliceType)))
        {
            mfxU32 luma_log2_weight_denom = 0;
            obs.PutUe(luma_log2_weight_denom);
            if (sps->chroma_format_idc != 0)
            {
                mfxU32 chroma_log2_weight_denom = 0;
                obs.PutUe(chroma_log2_weight_denom);
            }
            for (mfxU32 refindex = 0; refindex < numRefIdxL0Active; refindex++)
            {
                mfxU8 luma_weight_flag = 0;
                obs.PutBit(luma_weight_flag);

                if (sps->chroma_format_idc != 0)
                {
                    mfxU8 chroma_weight_flag = 0;
                    obs.PutBit(chroma_weight_flag);
                }
            }
            if (BPREDSLICE == sliceType)
            {
                for (mfxU32 refindex = 0; refindex < numRefIdxL1Active; refindex++)
                {
                    mfxU8 luma_weight_flag = 0;
                    obs.PutBit(luma_weight_flag);

                    if (sps->chroma_format_idc != 0)
                    {
                        mfxU8 chroma_weight_flag = 0;
                        obs.PutBit(chroma_weight_flag);
                    }
                }
            }
        }

        if (nalRefIdc)
        {
            // WriteDecRefPicMarking
            if (idrPicFlag)
            {
                obs.PutBit(sliceParams.ref_pic_fields.bits.no_output_of_prior_pics_flag);
                obs.PutBit(sliceParams.ref_pic_fields.bits.long_term_reference_flag);
            }
            else
            {
                obs.PutBit(sliceParams.ref_pic_fields.bits.adaptive_ref_pic_marking_mode_flag);
                if (sliceParams.ref_pic_fields.bits.adaptive_ref_pic_marking_mode_flag)
                {
                    mfxU32 num_entries = 0;
                    for (mfxU32 i = 0; i < MAX_NUM_REF_FRAMES; i++)
                    {
                        if (sliceParams.memory_management_control_operation[i])
                        {
                            MFX_DEBUG_TRACE_I32(sliceParams.memory_management_control_operation[i]);
                            obs.PutUe(sliceParams.memory_management_control_operation[i]);

                            switch (sliceParams.memory_management_control_operation[i])
                            {
                                case 1:
                                    MFX_DEBUG_TRACE_I32(sliceParams.difference_of_pic_nums_minus1[i]);
                                    obs.PutUe(sliceParams.difference_of_pic_nums_minus1[i]);
                                    break;
                                case 2:
                                    MFX_DEBUG_TRACE_I32(sliceParams.long_term_pic_num[i]);
                                    obs.PutUe(sliceParams.long_term_pic_num[i]);
                                    break;
                                case 3:
                                    MFX_DEBUG_TRACE_I32(sliceParams.difference_of_pic_nums_minus1[i]);
                                    MFX_DEBUG_TRACE_I32(sliceParams.long_term_frame_idx[i]);
                                    obs.PutUe(sliceParams.difference_of_pic_nums_minus1[i]);
                                    obs.PutUe(sliceParams.long_term_frame_idx[i]);
                                    break;
                                case 4:
                                    MFX_DEBUG_TRACE_I32(sliceParams.max_long_term_frame_idx_plus1[i]);
                                    obs.PutUe(sliceParams.max_long_term_frame_idx_plus1[i]);
                                    break;
                                case 5:
                                    // Mark all as unused for reference
                                    break;
                                case 6:
                                    MFX_DEBUG_TRACE_I32(sliceParams.long_term_frame_idx[i]);
                                    obs.PutUe(sliceParams.long_term_frame_idx[i]);
                                    break;
                                default:
                                    // invalid mmco command in bitstream
                                    MFX_DEBUG_TRACE_MSG("ERROR: invalid MMCO value");
                                    mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
                            }
                            if (MFX_ERR_NONE != mfx_res)
                                break;
                            num_entries++;
                        }
                    }
                    if (sliceParams.ref_pic_fields.bits.dec_ref_pic_marking_count != num_entries)
                    {
                        MFX_DEBUG_TRACE_MSG("ERROR: invalid MMCO number");
                        mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
                    }

                    obs.PutUe(0); //MMCO_END
                }
            }
        }

        if (pps->entropy_coding_mode && sliceType != INTRASLICE)
        {
            obs.PutUe(cabacInitIdc);
        }
        obs.PutSe(slice_qp_delta);

        if (pps->deblocking_filter_variables_present_flag)
        {
            mfxU32 disableDeblockingFilterIdc = 0;
            mfxI32 sliceAlphaC0OffsetDiv2     = 0;
            mfxI32 sliceBetaOffsetDiv2        = 0;

            obs.PutUe(disableDeblockingFilterIdc);
            if (disableDeblockingFilterIdc != 1)
            {
                obs.PutSe(sliceAlphaC0OffsetDiv2);
                obs.PutSe(sliceBetaOffsetDiv2);
            }
        }
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxU8* MfxC2AVCSecureFrameConstructor::GetMemoryForSwapping(mfxU32 size)
{
    MFX_DEBUG_TRACE_FUNC;
    try
    {
        if (m_swappingMemory.size() <= size + 8)
            m_swappingMemory.resize(size + 8);
    }
    catch (...)
    {
        return NULL;
    }

    return &(m_swappingMemory[0]);
}


MfxC2HEVCSecureFrameConstructor::MfxC2HEVCSecureFrameConstructor():
                                MfxC2AVCSecureFrameConstructor(), previous_poc(0)
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2HEVCSecureFrameConstructor::~MfxC2HEVCSecureFrameConstructor(void)
{
    MFX_DEBUG_TRACE_FUNC;
}

IMfxC2FrameConstructor::StartCode MfxC2HEVCSecureFrameConstructor::ReadStartCode(const mfxU8** position, mfxU32& size_left)
{
    MFX_DEBUG_TRACE_FUNC;

    return MfxC2HEVCFrameConstructor::ReadStartCode(position, size_left);
}

bool MfxC2HEVCSecureFrameConstructor::isRegularSlice(mfxI32 code)
{
    MFX_DEBUG_TRACE_FUNC;

    return (NAL_UT_HEVC_SLICE_TRAIL_N == code) ||
           (NAL_UT_HEVC_SLICE_TRAIL_R == code) ||
           (NAL_UT_HEVC_SLICE_TSA_N == code) ||
           (NAL_UT_HEVC_SLICE_TLA_R == code) ||
           (NAL_UT_HEVC_SLICE_STSA_N == code) ||
           (NAL_UT_HEVC_SLICE_STSA_R == code) ||
           (NAL_UT_HEVC_SLICE_RADL_N == code) ||
           (NAL_UT_HEVC_SLICE_RADL_R == code) ||
           (NAL_UT_HEVC_SLICE_RASL_N == code) ||
           (NAL_UT_HEVC_SLICE_RASL_R == code) ||
           (NAL_UT_HEVC_SLICE_BLA_W_LP == code) ||
           (NAL_UT_HEVC_SLICE_BLA_W_RADL == code) ||
           (NAL_UT_HEVC_SLICE_BLA_N_LP == code) ||
           (NAL_UT_HEVC_SLICE_CRA == code);
}

mfxStatus MfxC2HEVCSecureFrameConstructor::ParseNalUnit(mfxU8 * const data, mfxU32 NAlUnitSize)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    using namespace HEVCParser;

    mfxU32 swappingSize = NAlUnitSize;
    mfxU8 *swappingMemory = GetMemoryForSwapping(swappingSize);

    if (swappingMemory)
        BytesSwapper::SwapMemory(swappingMemory, swappingSize, data, NAlUnitSize);
    else
        mfx_res = MFX_ERR_MEMORY_ALLOC;

    if (MFX_ERR_NONE == mfx_res)
    {
        HEVCHeadersBitstream bitStream;
        MFX_DEBUG_TRACE_MSG("Calling bitStream.Reset()");
        bitStream.Reset(swappingMemory, swappingSize);

        NalUnitType nalUnitType = NAL_UT_INVALID;
        mfxU32 nuh_temporal_id;

        MFX_DEBUG_TRACE_MSG("Calling bitStream.GetNALUnitType()");
        bitStream.GetNALUnitType(nalUnitType, nuh_temporal_id);

        if (NAL_UT_SPS == nalUnitType)
        {
            MFX_DEBUG_TRACE_MSG("Found SPS");
            m_H265Headers.Reset();

            H265SeqParamSet sps;
            mfx_res = bitStream.GetSequenceParamSet(&sps);
            if (MFX_ERR_NONE == mfx_res)
            {
                m_H265Headers.m_seqParams.AddHeader(&sps);
                m_H265Headers.m_seqParams.SetCurrentID(sps.GetID());
            }
            else
            {
                MFX_DEBUG_TRACE_MSG("ERROR: Invalid SPS");
                mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else if (NAL_UT_PPS == nalUnitType)
        {
            MFX_DEBUG_TRACE_MSG("Found PPS");
            H265PicParamSet pps;
            // set illegal id
            pps.pps_pic_parameter_set_id = MAX_NUM_PIC_PARAM_SETS_H265;

            // Get id
            mfx_res = bitStream.GetPictureParamSetPart1(&pps);
            if (MFX_ERR_NONE == mfx_res)
            {
                H265SeqParamSet *pRefsps = m_H265Headers.m_seqParams.GetHeader(pps.pps_seq_parameter_set_id);
                // Get rest of pic param set
                mfx_res = bitStream.GetPictureParamSetFull(&pps, pRefsps);
                if (MFX_ERR_NONE == mfx_res)
                {
                    m_H265Headers.m_picParams.AddHeader(&pps);
                    m_H265Headers.m_picParams.SetCurrentID(pps.GetID());
                }
                else
                {
                    MFX_DEBUG_TRACE_MSG("ERROR: Invalid PPS");
                    mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
                }
            }
            else
            {
                MFX_DEBUG_TRACE_MSG("ERROR: Invalid PPS");
                mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
            }
        }
        else if (NAL_UT_SEI == nalUnitType)
        {
            MFX_DEBUG_TRACE_MSG("Found SEI");
            mfx_res = MFX_ERR_NONE;
        }
        else
        {
            MFX_DEBUG_TRACE_MSG("Found unknown NAL unit");
            MFX_DEBUG_TRACE_U32(nalUnitType);
            mfx_res = MFX_ERR_UNSUPPORTED;
        }
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

inline mfxU32 CeilLog2  (mfxU32 x)           { mfxU32 l = 0; while(x > (1U<<l)) l++; return l; }
inline mfxU32 CeilDiv   (mfxU32 x, mfxU32 y) { return (x + y - 1) / y; }

mfxStatus MfxC2HEVCSecureFrameConstructor::PackSliceHeader(OutputBitstream & obs, mfxU8* data, mfxU32 size)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    using namespace HEVCParser;

    H265SeqParamSet *sps = NULL;
    H265PicParamSet *pps = NULL;
    VACencSliceParameterBufferHEVC sliceParams{};

    pps = m_H265Headers.m_picParams.GetCurrentHeader();
    if (NULL == pps)
    {
        MFX_DEBUG_TRACE_MSG("ERROR: PPS not found");
        mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        sps = m_H265Headers.m_seqParams.GetCurrentHeader();
        if (NULL == sps)
        {
            MFX_DEBUG_TRACE_MSG("ERROR: SPS not found");
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        if (size != sizeof(VACencSliceParameterBufferHEVC))
        {
            MFX_DEBUG_TRACE_MSG("ERROR: incorrect slice_buf_size");
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }
    if (MFX_ERR_NONE == mfx_res)
    {
        sliceParams = *(reinterpret_cast<VACencSliceParameterBufferHEVC*>(data));

        MFX_DEBUG_TRACE_I32(sliceParams.nal_unit_type);
        MFX_DEBUG_TRACE_I32(sliceParams.nuh_temporal_id);
        MFX_DEBUG_TRACE_I32(sliceParams.slice_type);
        MFX_DEBUG_TRACE_I32(sliceParams.slice_pic_order_cnt_lsb);
        MFX_DEBUG_TRACE_I32(sliceParams.has_eos_or_eob);
        MFX_DEBUG_TRACE_I32(sliceParams.slice_fields.bits.no_output_of_prior_pics_flag);
        MFX_DEBUG_TRACE_I32(sliceParams.slice_fields.bits.pic_output_flag);
        MFX_DEBUG_TRACE_I32(sliceParams.slice_fields.bits.colour_plane_id);
        MFX_DEBUG_TRACE_I32(sliceParams.num_of_curr_before);
        MFX_DEBUG_TRACE_I32(sliceParams.num_of_curr_after);
        MFX_DEBUG_TRACE_I32(sliceParams.num_of_curr_total);
        MFX_DEBUG_TRACE_I32(sliceParams.num_of_foll_st);
        MFX_DEBUG_TRACE_I32(sliceParams.num_of_curr_lt);
        MFX_DEBUG_TRACE_I32(sliceParams.num_of_foll_lt);

        int32_t  current_poc = 0;
        uint32_t index_total = 0;
        uint8_t  ref_set_idx_curr_before[MAX_NUM_REF_PICS_CUR] = {};
        uint8_t  ref_set_idx_curr_after[MAX_NUM_REF_PICS_CUR] = {};
        uint8_t  ref_set_idx_curr_lt[MAX_NUM_REF_PICS_CUR] = {};

        uint8_t startcode[3] = { 0, 0, 1 };
        obs.PutRawBytes(startcode, startcode + sizeof startcode);
        obs.PutBit(0);
        obs.PutBits(sliceParams.nal_unit_type, 6);
        obs.PutBits(0, 6);                               // nuh_layer_id
        obs.PutBits(sliceParams.nuh_temporal_id + 1, 3);

        obs.PutBit(0); // first_slice_segment_in_pic_flag
        if (sliceParams.nal_unit_type >= NAL_UT_CODED_SLICE_BLA_W_LP && sliceParams.nal_unit_type <= NAL_RSV_IRAP_VCL23)
        {
            obs.PutBit(sliceParams.slice_fields.bits.no_output_of_prior_pics_flag);
        }

        obs.PutUe(pps->pps_pic_parameter_set_id);

        if (true) // first_slice_segment_in_pic_flag == 0
        {
            if (pps->dependent_slice_segments_enabled_flag)
            {
                obs.PutBit(0); // dependent_slice_segment_flag
            }
            uint32_t PicSizeInCtbsY = CeilDiv(sps->pic_width_in_luma_samples, sps->MaxCUSize) * CeilDiv(sps->pic_height_in_luma_samples, sps->MaxCUSize);
            int32_t slice_address_len = CeilLog2(PicSizeInCtbsY);

            obs.PutBits(0, // slice_segment_address
                        slice_address_len);
        }

        if (true) // dependent_slice_segment_flag == 0
        {
            if (pps->num_extra_slice_header_bits)
                obs.PutBits(0, pps->num_extra_slice_header_bits);

            obs.PutUe(sliceParams.slice_type);

            if (pps->output_flag_present_flag)
                obs.PutBit(sliceParams.slice_fields.bits.pic_output_flag);

            if (sps->separate_colour_plane_flag == 1)
                obs.PutBits(sliceParams.slice_fields.bits.colour_plane_id, 2);

            if (sliceParams.nal_unit_type != NAL_UT_CODED_SLICE_IDR_W_RADL &&
                sliceParams.nal_unit_type != NAL_UT_CODED_SLICE_IDR_N_LP)
            {
                obs.PutBits(sliceParams.slice_pic_order_cnt_lsb, sps->log2_max_pic_order_cnt_lsb);

                // decode poc
                int32_t cur_poc_lsb = sliceParams.slice_pic_order_cnt_lsb;
                int32_t max_poc_lsb = 1 << (sps->log2_max_pic_order_cnt_lsb);
                int32_t pre_poc_lsb = previous_poc & (max_poc_lsb - 1);
                int32_t pre_poc_msb = previous_poc - pre_poc_lsb;
                int32_t cur_poc_msb = 0;

                if ((cur_poc_lsb < pre_poc_lsb) && ((pre_poc_lsb - cur_poc_lsb) >= (max_poc_lsb / 2)))
                {
                    cur_poc_msb = pre_poc_msb + max_poc_lsb;
                }
                else if ((cur_poc_lsb > pre_poc_lsb) && ((cur_poc_lsb - pre_poc_lsb) > (max_poc_lsb / 2)))
                {
                    cur_poc_msb = pre_poc_msb - max_poc_lsb;
                }
                else
                {
                    cur_poc_msb = pre_poc_msb;
                }

                if ((NAL_UT_CODED_SLICE_BLA_W_LP == sliceParams.nal_unit_type) ||
                    (NAL_UT_CODED_SLICE_BLA_W_RADL == sliceParams.nal_unit_type) ||
                    (NAL_UT_CODED_SLICE_BLA_N_LP == sliceParams.nal_unit_type) )
                {
                    cur_poc_msb = 0;
                }

                current_poc = cur_poc_lsb + cur_poc_msb;
                MFX_DEBUG_TRACE_I32(current_poc);

                // short_term_rps
                obs.PutBit(0); // short_term_ref_pic_set_sps_flag
                if (true) // short_term_ref_pic_set_sps_flag == 0
                {
                    if (sps->getRPSList()->getNumberOfReferencePictureSets() > 0)
                    {
                        obs.PutBit(0); // rps->inter_ref_pic_set_prediction_flag
                    }

                    uint32_t num_negative_pics = 0;
                    uint32_t num_positive_pics = 0;
                    uint32_t i;
                    for(i = 0; i < sliceParams.num_of_curr_before; i++)
                    {
                        num_negative_pics++;
                    }
                    for(i = 0; i < sliceParams.num_of_curr_after; i++)
                    {
                        num_positive_pics++;
                    }
                    for(i = 0; i < sliceParams.num_of_foll_st; i++)
                    {
                        if (sliceParams.delta_poc_foll_st[i] < 0)
                            num_negative_pics++;
                        else
                            num_positive_pics++;
                    }

                    obs.PutUe(num_negative_pics);
                    obs.PutUe(num_positive_pics);

                    int32_t prev = 0;
                    int32_t poc;
                    uint8_t used_by_curr_pic_flag;
                    uint32_t index_foll = 0;
                    uint32_t delta_poc_s0_minus1;
                    for(i = 0; i < num_negative_pics; i++)
                    {
                        if (i < sliceParams.num_of_curr_before)
                        {
                            used_by_curr_pic_flag = 1;
                            poc = sliceParams.delta_poc_curr_before[i];
                            ref_set_idx_curr_before[i] = index_total;
                            index_total++;
                        }
                        else
                        {
                            used_by_curr_pic_flag = 0;
                            poc = sliceParams.delta_poc_foll_st[index_foll];
                            index_foll++;
                        }

                        delta_poc_s0_minus1 = prev - poc - 1;
                        prev = poc;
                        obs.PutUe(delta_poc_s0_minus1);
                        obs.PutBit(used_by_curr_pic_flag);
                    }

                    prev = 0;
                    uint32_t delta_poc_s1_minus1;
                    for(i = 0; i < num_positive_pics; i++)
                    {
                        if (i < sliceParams.num_of_curr_after)
                        {
                            used_by_curr_pic_flag = 1;
                            poc = sliceParams.delta_poc_curr_after[i];
                            ref_set_idx_curr_after[i] = index_total;
                            index_total++;
                        }
                        else
                        {
                            used_by_curr_pic_flag = 0;
                            poc = sliceParams.delta_poc_foll_st[index_foll];
                            index_foll++;
                        }

                        delta_poc_s1_minus1 = poc - prev - 1;
                        prev = poc;
                        obs.PutUe(delta_poc_s1_minus1);
                        obs.PutBit(used_by_curr_pic_flag);
                    }
                }
                else if (sps->num_short_term_ref_pic_sets > 1)
                {
                    int32_t len = CeilLog2(sps->num_short_term_ref_pic_sets);

                    obs.PutBits(0, // short_term_ref_pic_set_idx
                                len);
                }

                // long_term_ref_frames
                if (sps->long_term_ref_pics_present_flag)
                {
                    if (sps->num_long_term_ref_pics_sps > 0)
                    {
                        obs.PutUe(0); // num_long_term_sps
                    }
                    obs.PutUe(sliceParams.num_of_curr_lt + sliceParams.num_of_foll_lt);

                    uint32_t index_foll_lt = 0;
                    uint8_t used_flag_lt = 0;
                    int32_t poc_lsb_lt = 0;
                    int32_t poc_lsb_bit_mask = 0;
                    uint32_t i = 0;
                    for(i = 0; i < sliceParams.num_of_curr_lt + sliceParams.num_of_foll_lt; i++)
                    {
                        if (i < sliceParams.num_of_curr_lt)
                        {
                            used_flag_lt = 1;
                        }
                        else
                        {
                            used_flag_lt = 0;
                        }
                        if (sliceParams.delta_poc_msb_present_flag[i])
                        {
                            if (used_flag_lt)
                            {
                                poc_lsb_lt = sliceParams.delta_poc_curr_lt[i] + sliceParams.slice_pic_order_cnt_lsb;
                                ref_set_idx_curr_lt[i] = index_total;
                                index_total++;
                            }
                            else
                            {
                                poc_lsb_lt = sliceParams.delta_poc_foll_lt[index_foll_lt] + sliceParams.slice_pic_order_cnt_lsb;
                                index_foll_lt++;
                            }
                        }
                        else
                        {
                            if (used_flag_lt)
                            {
                                poc_lsb_lt = sliceParams.delta_poc_curr_lt[i] + current_poc;
                                ref_set_idx_curr_lt[i] = index_total;
                                index_total++;
                            }
                            else
                            {
                                poc_lsb_lt = sliceParams.delta_poc_foll_lt[index_foll_lt] + current_poc;
                                index_foll_lt++;
                            }

                            poc_lsb_bit_mask = (1 << sps->log2_max_pic_order_cnt_lsb) - 1;
                            poc_lsb_lt = poc_lsb_lt & poc_lsb_bit_mask;
                        }

                        obs.PutBits(poc_lsb_lt, sps->log2_max_pic_order_cnt_lsb);
                        obs.PutBit(used_flag_lt);
                        obs.PutBit(sliceParams.delta_poc_msb_present_flag[i]);
                        if (sliceParams.delta_poc_msb_present_flag[i])
                        {
                            obs.PutUe(0); // delta_poc_msb_cycle_lt
                        }
                    }
                }
                if (sps->sps_temporal_mvp_enabled_flag)
                    obs.PutBit(0); // slice_temporal_mvp_enabled_flag
            }
            else
            {
                current_poc  = 0;
                previous_poc = 0;
            }
            if (sps->sample_adaptive_offset_enabled_flag)
            {
                obs.PutBit(0); // slice_sao_luma_flag
                obs.PutBit(0); // slice_sao_chroma_flag
            }
            if (sliceParams.slice_type == P_SLICE || sliceParams.slice_type == B_SLICE)
            {
                uint8_t num_ref_idx[2] = {MAX_NUM_REF_PICS, MAX_NUM_REF_PICS};
                while((!sliceParams.ref_list_idx[0][num_ref_idx[0] - 1]) && (num_ref_idx[0] > 1))
                {
                    num_ref_idx[0]--;
                }
                while((!sliceParams.ref_list_idx[1][num_ref_idx[1] - 1]) && (num_ref_idx[1] > 1))
                {
                    num_ref_idx[1]--;
                }

                if (num_ref_idx[0] == pps->num_ref_idx_l0_default_active &&
                    num_ref_idx[1] == pps->num_ref_idx_l1_default_active)
                {
                    obs.PutBit(0); // num_ref_idx_active_override_flag
                }
                else
                {
                    obs.PutBit(1); // num_ref_idx_active_override_flag

                    obs.PutUe(num_ref_idx[0] - 1);
                    if (sliceParams.slice_type == B_SLICE)
                        obs.PutUe(num_ref_idx[1] - 1);
                }

                if ((pps->lists_modification_present_flag) && (sliceParams.num_of_curr_total > 1))
                {
                    // init_ref_lists
                    uint8_t i, j;
                    uint8_t num_ref_list_temp0, num_ref_list_temp1;
                    uint8_t ref_list_idx_rps[2][MAX_NUM_REF_PICS_CUR];
                    uint8_t ref_list_idx[2][MAX_NUM_REF_PICS];

                    num_ref_list_temp0 = num_ref_idx[0];
                    if (num_ref_list_temp0 < sliceParams.num_of_curr_total)
                    {
                        num_ref_list_temp0 = sliceParams.num_of_curr_total;
                    }

                    j = 0;
                    for(i = 0; i < num_ref_list_temp0; i++)
                    {
                        if (i < MAX_NUM_REF_PICS_CUR)
                        {
                            ref_list_idx_rps[0][i] = j;
                        }
                        ref_list_idx[0][i] = j;
                        j++;

                        if (j == sliceParams.num_of_curr_total)
                        {
                            j = 0;
                        }
                    }

                    if (B_SLICE == sliceParams.slice_type)
                    {
                        num_ref_list_temp1 = num_ref_idx[1];
                        if ( num_ref_list_temp1 < sliceParams.num_of_curr_total )
                        {
                            num_ref_list_temp1 = sliceParams.num_of_curr_total;
                        }

                        j = 0;
                        while (j < num_ref_list_temp1)
                        {
                            for(i = 0; (i < sliceParams.num_of_curr_after) && (j < num_ref_list_temp1); i++)
                            {
                                if (j < MAX_NUM_REF_PICS_CUR)
                                {
                                    ref_list_idx_rps[1][j] = ref_set_idx_curr_after[i];
                                }
                                ref_list_idx[1][j] = ref_set_idx_curr_after[i];
                                j++;
                            }
                            for(i = 0; (i < sliceParams.num_of_curr_before) && (j < num_ref_list_temp1); i++)
                            {
                                if (j < MAX_NUM_REF_PICS_CUR)
                                {
                                    ref_list_idx_rps[1][j] = ref_set_idx_curr_before[i];
                                }
                                ref_list_idx[1][j] = ref_set_idx_curr_before[i];
                                j++;
                            }
                            for(i = 0; (i < sliceParams.num_of_curr_lt) && (j < num_ref_list_temp1); i++)
                            {
                                if (j < MAX_NUM_REF_PICS_CUR)
                                {
                                    ref_list_idx_rps[1][j] = ref_set_idx_curr_lt[i];
                                }
                                ref_list_idx[1][j] = ref_set_idx_curr_lt[i];
                                j++;
                            }

                            if (j == 0)
                            {
                                break;
                            }
                        }
                    }

                    // ref_lists_modification
                    uint8_t bits_num = CeilLog2(sliceParams.num_of_curr_total);
                    if (memcmp(sliceParams.ref_list_idx[0], ref_list_idx[0], MAX_NUM_REF_PICS))
                    {
                        obs.PutBit(1); // slice_ref_pic_list_modification_flag_l0
                        for(i = 0; i < num_ref_idx[0]; i++)
                        {
                            uint32_t code = 0;
                            bool found = false;
                            for(j = 0; (j < num_ref_list_temp0) && (j < MAX_NUM_REF_PICS_CUR); j++)
                            {
                                if (sliceParams.ref_list_idx[0][i] == ref_list_idx_rps[0][j])
                                {
                                    code = j;
                                    found = true;
                                    break;
                                }
                            }
                            if (!found)
                                mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;

                            obs.PutBits(code, bits_num);
                        }
                    }
                    else
                    {
                        obs.PutBit(0); // slice_ref_pic_list_modification_flag_l0
                    }
                    if (B_SLICE == sliceParams.slice_type)
                    {
                        if (memcmp(sliceParams.ref_list_idx[1], ref_list_idx[1], MAX_NUM_REF_PICS))
                        {
                            obs.PutBit(1); // slice_ref_pic_list_modification_flag_l1
                            for(i = 0; i < num_ref_idx[1]; i++)
                            {
                                uint32_t code = 0;
                                bool found = false;
                                for(j = 0; (j < num_ref_list_temp1) && (j < MAX_NUM_REF_PICS_CUR); j++)
                                {
                                    if (sliceParams.ref_list_idx[1][i] == ref_list_idx_rps[1][j])
                                    {
                                        code = j;
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found)
                                    mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;

                                obs.PutBits(code, bits_num);
                            }
                        }
                        else
                        {
                            obs.PutBit(0); // slice_ref_pic_list_modification_flag_l1
                        }
                    }
                }

                if (sliceParams.slice_type == B_SLICE)
                    obs.PutBit(0); // mvd_l1_zero_flag
                if (pps->cabac_init_present_flag)
                    obs.PutBit(0); // cabac_init_flag

                if (false) // slice_temporal_mvp_enabled_flag
                {
                    if (sliceParams.slice_type == B_SLICE)
                        obs.PutBit(0); // collocated_from_l0_flag

                    mfxI32 collocated_from_l0_flag = 0; // collocated_from_l0_flag
                    if (sliceParams.slice_type != B_SLICE)
                        collocated_from_l0_flag = 1; // inferred

                    if (( collocated_from_l0_flag && num_ref_idx[0] > 1) ||
                        (!collocated_from_l0_flag && num_ref_idx[1] > 1))
                        obs.PutUe(0); // collocated_ref_idx
                }

                // pred_weight_table
                if ((pps->weighted_pred_flag && sliceParams.slice_type == P_SLICE)  ||
                    (pps->weighted_bipred_flag && sliceParams.slice_type == B_SLICE))
                {
                    uint8_t i;
                    uint8_t num_of_lists = (sliceParams.slice_type == B_SLICE ) ? (2) : (1);

                    obs.PutUe(0); // luma_log2_weight_denom
                    if (sps->ChromaArrayType)
                    {
                        obs.PutSe(0); // delta_chroma_log2_weight_denom
                    }

                    for(uint8_t list_num = 0; list_num < num_of_lists; list_num++ )
                    {
                        for(i = 0; i < num_ref_idx[list_num]; i++)
                        {
                            obs.PutBit(0); // luma_weight_lX_flag
                        }
                        if (sps->ChromaArrayType)
                        {
                            for(i = 0 ; i < num_ref_idx[list_num] ; i++)
                            {
                                obs.PutBit(0); // chroma_weight_lX_flag
                            }
                        }
                    }
                }

                obs.PutUe(0); // five_minus_max_num_merge_cand
            }

            obs.PutSe(0); // slice_qp_delta
            if (pps->pps_slice_chroma_qp_offsets_present_flag)
            {
                obs.PutSe(0); // slice_cb_qp_offset
                obs.PutSe(0); // slice_cr_qp_offset
            }
            if (pps->pps_slice_act_qp_offsets_present_flag)
            {
                obs.PutSe(0); // slice_act_y_qp_offset
                obs.PutSe(0); // slice_act_cb_qp_offset
                obs.PutSe(0); // slice_act_cr_qp_offset
            }
            if (pps->chroma_qp_offset_list_enabled_flag)
            {
                obs.PutBit(0); // cu_chroma_qp_offset_enabled_flag
            }
            if (pps->deblocking_filter_control_present_flag)
            {
                if (pps->deblocking_filter_override_enabled_flag)
                {
                    obs.PutBit(0); // deblocking_filter_override_flag
                }
                if (false) // deblocking_filter_override_flag
                {
                    obs.PutBit(0); // slice_deblocking_filter_disabled_flag
                    if (true) // !slice_deblocking_filter_disabled_flag
                    {
                        obs.PutSe(0); // slice_beta_offset_div2
                        obs.PutSe(0); // slice_tc_offset_div2
                    }
                }
            }
            if (pps->pps_loop_filter_across_slices_enabled_flag &&
                (false || // slice_sao_luma_flag
                 false || // slice_sao_chroma_flag
                 true))   // !slice_deblocking_filter_disabled_flag
            {
                obs.PutBit(0); // slice_loop_filter_across_slices_enabled_flag
            }
        } // !dependent_slice_segment_flag

        if (pps->tiles_enabled_flag || pps->entropy_coding_sync_enabled_flag)
        {
            obs.PutUe(0); // num_entry_point_offsets
        }
        if (pps->slice_segment_header_extension_present_flag)
        {
            obs.PutUe(0); // slice_header_extension_length
        }

        obs.PutBit(1);

        if (sliceParams.nuh_temporal_id == 0)
        {
            if (((sliceParams.nal_unit_type <= NAL_RSV_VCL_R15) && ((sliceParams.nal_unit_type %2) !=0 )) ||
                ((sliceParams.nal_unit_type > NAL_UT_CODED_SLICE_BLA_W_LP) && (sliceParams.nal_unit_type <= NAL_RSV_IRAP_VCL23)))
            {
                if ((sliceParams.nal_unit_type != NAL_UT_CODED_SLICE_RADL_R) &&
                    (sliceParams.nal_unit_type != NAL_UT_CODED_SLICE_RASL_R))
                {
                    previous_poc = current_poc;
                }
            }
        }
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

#endif
