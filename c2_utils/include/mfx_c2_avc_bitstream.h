// Copyright (c) 2013-2021 Intel Corporation
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

#ifndef __MFX_C2_AVC_BITSTREAM_H_
#define __MFX_C2_AVC_BITSTREAM_H_

#include "mfx_c2_avc_headers.h"
#include "mfx_c2_bs_utils.h"

namespace AVCParser
{

#define AVCPeek1Bit(current_data, offset) \
    ((current_data[0] >> (offset)) & 1)

#define AVCDrop1Bit(current_data, offset) \
{ \
    offset -= 1; \
    if (offset < 0) \
    { \
        offset = 31; \
        current_data += 1; \
    } \
}

// NAL unit definitions
enum
{
    NAL_STORAGE_IDC_BITS   = 0x60,
    NAL_UNITTYPE_BITS      = 0x1f
};

class AVCBaseBitstream
{
public:

    AVCBaseBitstream();
    AVCBaseBitstream(mfxU8 * const pb, const mfxU32 maxsize);
    virtual ~AVCBaseBitstream();

    // Reset the bitstream with new data pointer
    void Reset(mfxU8 * const pb, mfxU32 maxsize);
    void Reset(mfxU8 * const pb, mfxI32 offset, mfxU32 maxsize);

    inline mfxU32 GetBits(mfxU32 nbits);

    // Read one VLC mfxI32 or mfxU32 value from bitstream
    mfxI32 GetVLCElement(bool bIsSigned);

    // Reads one bit from the buffer.
    inline mfxU32 Get1Bit();

    // Check amount of data
    bool More_RBSP_Data();

    inline mfxU32 BytesDecoded();

    inline mfxU32 BitsDecoded();

    inline mfxU32 BytesLeft();

    mfxStatus GetNALUnitType(NAL_Unit_Type &uNALUnitType, mfxU8 &uNALStorageIDC);
    void AlignPointerRight(void);

protected:
    mfxU32 *m_pbs;                                              // pointer to the current position of the buffer.
    mfxI32 m_nBitOffset;                                         // the bit position (0 to 31) in the dword pointed by m_pbs.
    mfxU32 *m_pbsBase;                                          // pointer to the first byte of the buffer.
    mfxU32 m_uMaxBsSize;                                         // maximum buffer size in bytes.
};

class AVCHeadersBitstream : public AVCBaseBitstream
{
public:

    AVCHeadersBitstream();
    AVCHeadersBitstream(mfxU8 * const pb, const mfxU32 maxsize);


    // Decode sequence parameter set
    mfxStatus GetSequenceParamSet(AVCSeqParamSet *sps);
    // Decode sequence parameter set extension
    mfxStatus GetSequenceParamSetExtension(AVCSeqParamSetExtension *sps_ex);

    // Decoding picture's parameter set functions
    mfxStatus GetPictureParamSetPart1(AVCPicParamSet *pps);
    mfxStatus GetPictureParamSetPart2(AVCPicParamSet *pps, const AVCSeqParamSet *sps);

    mfxStatus GetSliceHeaderPart1(AVCSliceHeader *pSliceHeader);
    // Decoding slice header functions
    mfxStatus GetSliceHeaderPart2(AVCSliceHeader *hdr, // slice header read goes here
                               const AVCPicParamSet *pps,
                               const AVCSeqParamSet *sps); // from slice header NAL unit

    mfxStatus GetSliceHeaderPart3(AVCSliceHeader *hdr, // slice header read goes here
                               PredWeightTable *pPredWeight_L0, // L0 weight table goes here
                               PredWeightTable *pPredWeight_L1, // L1 weight table goes here
                               RefPicListReorderInfo *pReorderInfo_L0,
                               RefPicListReorderInfo *pReorderInfo_L1,
                               AdaptiveMarkingInfo *pAdaptiveMarkingInfo,
                               const AVCPicParamSet *pps,
                               const AVCSeqParamSet *sps,
                               mfxU8 NALRef_idc); // from slice header NAL unit


    mfxStatus GetNalUnitPrefix(AVCNalExtension *pExt, mfxU32 NALRef_idc);

    mfxI32 GetSEI(const HeaderSet<AVCSeqParamSet> & sps, mfxI32 current_sps, AVCSEIPayLoad *spl);

private:

    mfxStatus GetNalUnitExtension(AVCNalExtension *pExt);

    void GetScalingList4x4(AVCScalingList4x4 *scl, mfxU8 *def, mfxU8 *scl_type);
    void GetScalingList8x8(AVCScalingList8x8 *scl, mfxU8 *def, mfxU8 *scl_type);

    mfxI32 GetSEIPayload(const HeaderSet<AVCSeqParamSet> & sps, mfxI32 current_sps, AVCSEIPayLoad *spl);
    mfxI32 recovery_point(const HeaderSet<AVCSeqParamSet> & sps, mfxI32 current_sps, AVCSEIPayLoad *spl);
    mfxI32 reserved_sei_message(const HeaderSet<AVCSeqParamSet> & sps, mfxI32 current_sps, AVCSEIPayLoad *spl);

    mfxStatus GetVUIParam(AVCSeqParamSet *sps);
    mfxStatus GetHRDParam(AVCSeqParamSet *sps);
};


void SetDefaultScalingLists(AVCSeqParamSet * sps);

extern const mfxU32 bits_data[];


#define _avcGetBits(current_data, offset, nbits, data) \
{ \
    mfxU32 x; \
 \
    SAMPLE_ASSERT((nbits) > 0 && (nbits) <= 32); \
    SAMPLE_ASSERT(offset >= 0 && offset <= 31); \
 \
    offset -= (nbits); \
 \
    if (offset >= 0) \
    { \
        x = current_data[0] >> (offset + 1); \
    } \
    else \
    { \
        offset += 32; \
 \
        x = current_data[1] >> (offset); \
        x >>= 1; \
        x += current_data[0] << (31 - offset); \
        current_data++; \
    } \
 \
    SAMPLE_ASSERT(offset >= 0 && offset <= 31); \
 \
    (data) = x & bits_data[nbits]; \
}

#define avcGetBits1(current_data, offset, data) \
{ \
    data = ((current_data[0] >> (offset)) & 1);  \
    offset -= 1; \
    if (offset < 0) \
    { \
        offset = 31; \
        current_data += 1; \
    } \
}

#define avcUngetNBits(current_data, offset, nbits) \
{ \
    SAMPLE_ASSERT(offset >= 0 && offset <= 31); \
 \
    offset += (nbits); \
    if (offset > 31) \
    { \
        offset -= 32; \
        current_data--; \
    } \
 \
    SAMPLE_ASSERT(offset >= 0 && offset <= 31); \
}

#define avcGetNBits( current_data, offset, nbits, data) \
    _avcGetBits(current_data, offset, nbits, data);

#define avcNextBits(current_data, bp, nbits, data) \
{ \
    mfxU32 x; \
 \
    SAMPLE_ASSERT((nbits) > 0 && (nbits) <= 32); \
    SAMPLE_ASSERT(nbits >= 0 && nbits <= 31); \
 \
    mfxI32 offset = bp - (nbits); \
 \
    if (offset >= 0) \
    { \
        x = current_data[0] >> (offset + 1); \
    } \
    else \
    { \
        offset += 32; \
 \
        x = current_data[1] >> (offset); \
        x >>= 1; \
        x += current_data[0] << (31 - offset); \
    } \
 \
    SAMPLE_ASSERT(offset >= 0 && offset <= 31); \
 \
    (data) = x & bits_data[nbits]; \
}

inline mfxU32 AVCBaseBitstream::GetBits(mfxU32 nbits)
{
    mfxU32 w, n = nbits;

    avcGetNBits(m_pbs, m_nBitOffset, n, w);
    return w;
}

inline mfxU32 AVCBaseBitstream::Get1Bit()
{
    mfxU32 w;
    avcGetBits1(m_pbs, m_nBitOffset, w);
    return w;

} // AVCBitstream::Get1Bit()

inline mfxU32 AVCBaseBitstream::BytesDecoded()
{
    return static_cast<mfxU32>((mfxU8*)m_pbs - (mfxU8*)m_pbsBase) +
            ((31 - m_nBitOffset) >> 3);
}

inline mfxU32 AVCBaseBitstream::BytesLeft()
{
    return ((mfxI32)m_uMaxBsSize - (mfxI32) BytesDecoded());
}

} // namespace AVCParser

#endif // __MFX_C2_AVC_BITSTREAM_H_
