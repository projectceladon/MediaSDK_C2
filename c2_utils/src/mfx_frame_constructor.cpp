/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_frame_constructor.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_frame_constructor"

MfxC2FrameConstructor::MfxC2FrameConstructor():
    bs_state_(MfxC2BS_HeaderAwaiting),
    profile_(MFX_PROFILE_UNKNOWN),
    eos_(false),
    bst_buf_reallocs_(0),
    bst_buf_copy_bytes_(0)
{
    MFX_DEBUG_TRACE_FUNC;

    bst_header_ = std::make_shared<mfxBitstream>();
    bst_buf_ = std::make_shared<mfxBitstream>();
    bst_in_ = std::make_shared<mfxBitstream>();

    MFX_ZERO_MEMORY((*bst_header_));
    MFX_ZERO_MEMORY((*bst_buf_));
    MFX_ZERO_MEMORY((*bst_in_));
    MFX_ZERO_MEMORY(fr_info_);
}

MfxC2FrameConstructor::~MfxC2FrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    if (bst_buf_->Data) {
        MFX_DEBUG_TRACE_I32(bst_buf_->MaxLength);
        MFX_DEBUG_TRACE_I32(bst_buf_reallocs_);
        MFX_DEBUG_TRACE_I32(bst_buf_copy_bytes_);

        MFX_FREE(bst_buf_->Data);
    }

    MFX_FREE(bst_header_->Data);
}

mfxStatus MfxC2FrameConstructor::Init(
    mfxU16 profile,
    mfxFrameInfo fr_info )
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    profile_ = profile;
    fr_info_ = fr_info;
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2FrameConstructor::LoadHeader(const mfxU8* data, mfxU32 size, bool header)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_DEBUG_TRACE_P(data);
    MFX_DEBUG_TRACE_I32(size);
    if (!data || !size) mfx_res = MFX_ERR_NULL_PTR;
    if (MFX_ERR_NONE == mfx_res) {
        if (header) {
            // if new header arrived after reset we are ignoring previously collected header data
            if (bs_state_ == MfxC2BS_Resetting) {
                bs_state_ = MfxC2BS_HeaderObtained;
            } else if (size) {
                mfxU32 needed_MaxLength = 0;
                mfxU8* new_data = nullptr;

                needed_MaxLength = bst_header_->DataOffset + bst_header_->DataLength + size; // offset should be 0
                if (bst_header_->MaxLength < needed_MaxLength) {
                    // increasing buffer capacity if needed
                    new_data = (mfxU8*)realloc(bst_header_->Data, needed_MaxLength);
                    if (new_data) {
                        // setting new values
                        bst_header_->Data = new_data;
                        bst_header_->MaxLength = needed_MaxLength;
                    }
                    else mfx_res = MFX_ERR_MEMORY_ALLOC;
                }
                if (MFX_ERR_NONE == mfx_res) {
                    mfxU8* buf = bst_header_->Data + bst_header_->DataOffset + bst_header_->DataLength;

                    memcpy(buf, data, size);
                    bst_header_->DataLength += size;
                }
                if (MfxC2BS_HeaderAwaiting == bs_state_) bs_state_ = MfxC2BS_HeaderCollecting;
            }
        } else {
            // We have generic data. In case we are in Resetting state (i.e. seek mode)
            // we attach header to the bitstream, other wise we are moving in Obtained state.
            if (MfxC2BS_HeaderCollecting == bs_state_) {
                // As soon as we are receving first non header data we are stopping collecting header
                bs_state_ = MfxC2BS_HeaderObtained;
            }
            else if (MfxC2BS_Resetting == bs_state_) {
                // if reset detected and we have header data buffered - we are going to load it
                mfx_res = BstBufRealloc(bst_header_->DataLength);
                if (MFX_ERR_NONE == mfx_res) {
                    mfxU8* buf = bst_buf_->Data + bst_buf_->DataOffset + bst_buf_->DataLength;

                    memcpy(buf, bst_header_->Data + bst_header_->DataOffset, bst_header_->DataLength);
                    bst_buf_->DataLength += bst_header_->DataLength;
                    bst_buf_copy_bytes_ += bst_header_->DataLength;
                }
                bs_state_ = MfxC2BS_HeaderObtained;
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
    if ((MFX_ERR_NONE == mfx_res) && bst_buf_->DataLength) {
        mfx_res = BstBufRealloc(size);
        if (MFX_ERR_NONE == mfx_res) {
            mfxU8* buf = bst_buf_->Data + bst_buf_->DataOffset + bst_buf_->DataLength;

            memcpy(buf, data, size);
            bst_buf_->DataLength += size;
            bst_buf_copy_bytes_ += size;
        }
    }
    if (MFX_ERR_NONE == mfx_res) {
        if (bst_buf_->DataLength) bst_current_ = bst_buf_;
        else {
            bst_in_->Data = (mfxU8*)data;
            bst_in_->DataOffset = 0;
            bst_in_->DataLength = size;
            bst_in_->MaxLength = size;
            if (complete_frame)
                bst_in_->DataFlag |= MFX_BITSTREAM_COMPLETE_FRAME;

            bst_current_ = bst_in_;
        }
        bst_current_->TimeStamp = pts;
    }
    else bst_current_ = nullptr;
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
    MFX_DEBUG_TRACE__mfxBitstream((*bst_buf_));
    MFX_DEBUG_TRACE__mfxBitstream((*bst_in_));
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
    mfxU8* data = bst_buf_->Data;
    mfxU32 allocated_length = bst_buf_->MaxLength;

    // resetting frame constructor
    bst_current_ = nullptr;
    bst_buf_ = std::make_shared<mfxBitstream>();
    MFX_ZERO_MEMORY((*bst_buf_));
    bst_in_ = std::make_shared<mfxBitstream>();
    MFX_ZERO_MEMORY((*bst_in_));

    eos_ = false;

    // restoring allocating information about internal buffer
    bst_buf_->Data = data;
    bst_buf_->MaxLength = allocated_length;

    // we have some header data and will attempt to return it
    if (bs_state_ >= MfxC2BS_HeaderCollecting) bs_state_ = MfxC2BS_Resetting;

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
        needed_MaxLength = bst_buf_->DataOffset + bst_buf_->DataLength + add_size; // offset should be 0
        if (bst_buf_->MaxLength < needed_MaxLength) {
            // increasing buffer capacity if needed
            new_data = (mfxU8*)realloc(bst_buf_->Data, needed_MaxLength);
            if (new_data) {
                // collecting statistics
                ++bst_buf_reallocs_;
                if (new_data != bst_buf_->Data) bst_buf_copy_bytes_ += bst_buf_->MaxLength;
                // setting new values
                bst_buf_->Data = new_data;
                bst_buf_->MaxLength = needed_MaxLength;
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
        if (bst_buf_->MaxLength < needed_MaxLength) {
            // increasing buffer capacity if needed
            MFX_FREE(bst_buf_->Data);
            bst_buf_->Data = (mfxU8*)malloc(needed_MaxLength);
            bst_buf_->MaxLength = needed_MaxLength;
            ++bst_buf_reallocs_;
        }
        if (!(bst_buf_->Data)) {
            bst_buf_->MaxLength = 0;
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

    if (nullptr != bst_current_) {
        if (bst_current_ == bst_buf_) {
            if (bst_buf_->DataLength && bst_buf_->DataOffset) {
                // shifting data to the beginning of the buffer
                memmove(bst_buf_->Data, bst_buf_->Data + bst_buf_->DataOffset, bst_buf_->DataLength);
                bst_buf_copy_bytes_ += bst_buf_->DataLength;
            }
            bst_buf_->DataOffset = 0;
        }
        if ((bst_current_ == bst_in_) && bst_in_->DataLength) {
            // copying data from bst_in_ to bst_Buf
            // Note: we read data from bst_in_, thus here bst_Buf is empty
            mfx_res = BstBufMalloc(bst_in_->DataLength);
            if (MFX_ERR_NONE == mfx_res) {
                memcpy(bst_buf_->Data, bst_in_->Data + bst_in_->DataOffset, bst_in_->DataLength);
                bst_buf_->DataOffset = 0;
                bst_buf_->DataLength = bst_in_->DataLength;
                bst_buf_->TimeStamp  = bst_in_->TimeStamp;
                bst_buf_->DataFlag   = bst_in_->DataFlag;
                bst_buf_copy_bytes_ += bst_in_->DataLength;
            }
            bst_in_ = std::make_shared<mfxBitstream>();
            MFX_ZERO_MEMORY((*bst_in_));
        }
        bst_current_ = nullptr;
    }
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

std::shared_ptr<mfxBitstream> MfxC2FrameConstructor::GetMfxBitstream()
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<mfxBitstream> bst;

    if (bst_buf_->Data && bst_buf_->DataLength) {
        bst = bst_buf_;
    } else if (bst_in_->Data && bst_in_->DataLength) {
        bst = bst_in_;
    } else {
        bst = bst_buf_;
    }

    MFX_DEBUG_TRACE_P(bst_in_.get());
    MFX_DEBUG_TRACE_P(bst_buf_.get());
    MFX_DEBUG_TRACE_P(bst.get());
    return bst;
}

MfxC2AVCFrameConstructor::MfxC2AVCFrameConstructor():
    MfxC2FrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_ZERO_MEMORY(sps_);
    MFX_ZERO_MEMORY(pps_);
}

MfxC2AVCFrameConstructor::~MfxC2AVCFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_FREE(sps_.Data);
    MFX_FREE(pps_.Data);
}

mfxStatus MfxC2AVCFrameConstructor::SaveHeaders(std::shared_ptr<mfxBitstream> sps, std::shared_ptr<mfxBitstream> pps, bool is_reset)
{
    MFX_DEBUG_TRACE_FUNC;

    if (is_reset) Reset();

    if (nullptr != sps) {
        if (sps_.MaxLength < sps->DataLength) {
            sps_.Data = (mfxU8*)realloc(sps_.Data, sps->DataLength);
            if (!sps_.Data)
                return MFX_ERR_MEMORY_ALLOC;
            sps_.MaxLength = sps->DataLength;
        }
        memcpy(sps_.Data, sps->Data + sps->DataOffset, sps->DataLength);
        sps_.DataLength = sps->DataLength;
    }
    if (nullptr != pps) {
        if (pps_.MaxLength < pps->DataLength) {
            pps_.Data = (mfxU8*)realloc(pps_.Data, pps->DataLength);
            if (!pps_.Data)
                return MFX_ERR_MEMORY_ALLOC;
            pps_.MaxLength = pps->DataLength;
        }
        memcpy(pps_.Data, pps->Data + pps->DataOffset, pps->DataLength);
        pps_.DataLength = pps->DataLength;
    }
    return MFX_ERR_NONE;
}

mfxStatus MfxC2AVCFrameConstructor::FindHeaders(const mfxU8* data, mfxU32 size, bool &found_sps, bool &found_pps)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    found_sps = false;
    found_pps = false;

    if (data && size) {
        StartCode start_code;
        mfxU32 length;
        for (; size > 3;) {
            start_code = ReadStartCode(&data, &size);
            if (isSPS(start_code.type)) {
                std::shared_ptr<mfxBitstream> sps = std::make_shared<mfxBitstream>();
                MFX_ZERO_MEMORY((*sps));
                sps->Data = (mfxU8*)data - start_code.size;

                length = size + start_code.size;
                start_code = ReadStartCode(&data, &size);
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
                MFX_ZERO_MEMORY((*pps));
                pps->Data = (mfxU8*)data - start_code.size;

                length = size + start_code.size;
                start_code = ReadStartCode(&data, &size);
                if (-1 != start_code.type)
                    length -= size + start_code.size;
                pps->DataLength = length;
                MFX_DEBUG_TRACE_STREAM("Found PPS size " << length);
                mfx_res = SaveHeaders(nullptr, pps, false);
                if (MFX_ERR_NONE != mfx_res) return mfx_res;
                found_pps = true;
            }
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

    bool bFoundSps = false;
    bool bFoundPps = false;

    if (header && data && size) {
        if (MfxC2BS_HeaderAwaiting == bs_state_) bs_state_ = MfxC2BS_HeaderCollecting;

        mfx_res = FindHeaders(data, size, bFoundSps, bFoundPps);
        if (MFX_ERR_NONE == mfx_res && bFoundSps && bFoundPps)
            bs_state_ = MfxC2BS_HeaderObtained;

    } else if (MfxC2BS_Resetting == bs_state_) {
        mfx_res = FindHeaders(data, size, bFoundSps, bFoundPps);
        if (MFX_ERR_NONE == mfx_res) {
            if (!bFoundSps || !bFoundPps) {
                // In case we are in Resetting state (i.e. seek mode)
                // and bitstream has no headers, we attach header to the bitstream.
                mfx_res = BstBufRealloc(sps_.DataLength + pps_.DataLength);
                if (MFX_ERR_NONE == mfx_res) {
                    mfxU8* buf = bst_buf_->Data + bst_buf_->DataOffset + bst_buf_->DataLength;
                    memcpy(buf, sps_.Data, sps_.DataLength);
                    buf += sps_.DataLength;
                    memcpy(buf, pps_.Data, pps_.DataLength);

                    bst_buf_->DataLength += sps_.DataLength + pps_.DataLength;
                    bst_buf_copy_bytes_ += sps_.DataLength + pps_.DataLength;
                }
            }
            bs_state_ = MfxC2BS_HeaderObtained;
        }
    } else if (MfxC2BS_HeaderCollecting == bs_state_) {
        // As soon as we are receving first non header data we are stopping collecting header
        bs_state_ = MfxC2BS_HeaderObtained;
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

IMfxC2FrameConstructor::StartCode MfxC2AVCFrameConstructor::ReadStartCode(const mfxU8** position, mfxU32* size_left)
{
    MFX_DEBUG_TRACE_FUNC;

    StartCode start_code = { .type=-1, .size=0 };
    mfxU32 zero_count = 0;
    static const mfxU8 nal_unit_type_bits = 0x1f;

    mfxI32 i = 0;
    for (; i < (mfxI32)*size_left - 2; ) {
        if ((*position)[1]) {
            *position += 2;
            i += 2;
            continue;
        }

        zero_count = 0;
        if (!(*position)[0]) zero_count++;

        mfxU32 j;
        for (j = 1; j < (mfxU32)*size_left - i; j++) {
            if ((*position)[j]) break;
        }

        zero_count = zero_count ? j: j - 1;

        *position += j;
        i += j;

        if (i >= (mfxI32)*size_left) break;

        if (zero_count >= 2 && (*position)[0] == 1) {
            start_code.size = MFX_MIN(zero_count + 1, 4);
            *size_left -= i + 1;
            (*position)++; // remove 0x01 symbol
            if (*size_left >= 1) {
                start_code.type = (*position)[0] & nal_unit_type_bits;
            } else {
                *position -= start_code.size;
                *size_left += start_code.size;
                start_code.size = 0;
            }
            return start_code;
        }
        zero_count = 0;
    }

    if (!zero_count) {
        for (mfxU32 k = 0; k < *size_left - i; k++, (*position)++) {
            if ((*position)[0]) {
                zero_count = 0;
                continue;
            }
            zero_count++;
        }
    }

    zero_count = MFX_MIN(zero_count, 3);
    *position -= zero_count;
    *size_left = zero_count;
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

IMfxC2FrameConstructor::StartCode MfxC2HEVCFrameConstructor::ReadStartCode(const mfxU8** position, mfxU32* size_left)
{
    MFX_DEBUG_TRACE_FUNC;

    StartCode start_code = { .type=-1, .size=0 };
    mfxU32 zero_count = 0;
    static const mfxU8 NAL_UNITTYPE_BITS_H265 = 0x7e;
    static const mfxU8 NAL_UNITTYPE_SHIFT_H265 = 1;

    mfxI32 i = 0;
    for (; i < (mfxI32)*size_left - 2; ) {
        if ((*position)[1]) {
            *position += 2;
            i += 2;
            continue;
        }

        zero_count = 0;
        if (!(*position)[0]) zero_count++;

        mfxU32 j;
        for (j = 1; j < (mfxU32)*size_left - i; j++) {
            if ((*position)[j]) break;
        }

        zero_count = zero_count ? j: j - 1;

        *position += j;
        i += j;

        if (i >= (mfxI32)*size_left) break;

        if (zero_count >= 2 && (*position)[0] == 1) {
            start_code.size = MFX_MIN(zero_count + 1, 4);
            *size_left -= i + 1;
            (*position)++; // remove 0x01 symbol
            if (*size_left >= 1) {
                start_code.type = ((*position)[0] & NAL_UNITTYPE_BITS_H265) >> NAL_UNITTYPE_SHIFT_H265;
            } else {
                *position -= start_code.size;
                *size_left += start_code.size;
                start_code.size = 0;
            }
            return start_code;
        }
        zero_count = 0;
    }

    if (!zero_count) {
        for (mfxU32 k = 0; k < *size_left - i; k++, (*position)++) {
            if ((*position)[0]) {
                zero_count = 0;
                continue;
            }
            zero_count++;
        }
    }

    zero_count = MFX_MIN(zero_count, 3);
    *position -= zero_count;
    *size_left = zero_count;
    return start_code;
}

std::shared_ptr<IMfxC2FrameConstructor> MfxC2FrameConstructorFactory::CreateFrameConstructor(MfxC2FrameConstructorType fc_type)
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<IMfxC2FrameConstructor> fc;
    if (MfxC2FC_AVC == fc_type) {
        fc = std::make_shared<MfxC2AVCFrameConstructor>();
        return fc;

    } else if (MfxC2FC_HEVC == fc_type) {
        fc = std::make_shared<MfxC2HEVCFrameConstructor>();
        return fc;

    } else {
        fc = std::make_shared<MfxC2FrameConstructor>();
        return fc;
    }
}
