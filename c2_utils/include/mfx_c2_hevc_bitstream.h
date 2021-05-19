// Copyright (c) 2018-2021 Intel Corporation
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

#ifndef __MFX_C2_HEVC_BITSTREAM_H_
#define __MFX_C2_HEVC_BITSTREAM_H_

#include "mfx_c2_avc_bitstream.h"
#include "mfx_c2_hevc_headers.h"

namespace HEVCParser
{

#define bits_data AVCParser::bits_data

// Read N bits from 32-bit array
#define GetNBits( current_data, offset, nbits, data) \
    _avcGetBits(current_data, offset, nbits, data);

// Return bitstream position pointers N bits back
#define UngetNBits(current_data, offset, nbits) \
    avcUngetNBits(current_data, offset, nbits)

// Read N bits from 32-bit array
#define PeakNextBits(current_data, bp, nbits, data) \
    avcNextBits(current_data, bp, nbits, data)

class HEVCBaseBitstream : public AVCParser::AVCBaseBitstream
{
public:

    HEVCBaseBitstream() : AVCBaseBitstream() {}
    HEVCBaseBitstream(mfxU8 * const pb, const mfxU32 maxsize) : AVCBaseBitstream(pb, maxsize) {}
    virtual ~HEVCBaseBitstream() {}

    // Read variable length coded unsigned element
    uint32_t GetVLCElementU();

    // Read variable length coded signed element
    int32_t GetVLCElementS();
};

class HEVCHeadersBitstream : public HEVCBaseBitstream
{
public:

    HEVCHeadersBitstream() : HEVCBaseBitstream() {}
    HEVCHeadersBitstream(uint8_t * const pb, const uint32_t maxsize) : HEVCBaseBitstream(pb, maxsize) {}

    // Read and return NAL unit type and NAL storage idc.
    // Bitstream position is expected to be at the start of a NAL unit.
    mfxStatus GetNALUnitType(NalUnitType &nal_unit_type, uint32_t &nuh_temporal_id);

    // Parse scaling list information in SPS or PPS
    void parseScalingList(H265ScalingList *);
    // Reserved for future header extensions
    bool MoreRbspData();

    // Parse SPS header
    mfxStatus GetSequenceParamSet(H265SeqParamSet *sps);

    // Parse PPS header
    mfxStatus GetPictureParamSetPart1(H265PicParamSet *pps);
    mfxStatus GetPictureParamSetFull(H265PicParamSet *pps, H265SeqParamSet const*);

    void GetSEI(mfxPayload *spl, mfxU32 type);

    void parseShortTermRefPicSet(const H265SeqParamSet* sps, ReferencePictureSet* pRPS, uint32_t idx);

protected:

    // Parse video usability information block in SPS
    void parseVUI(H265SeqParamSet *sps);

    // Parse scaling list data block
    void xDecodeScalingList(H265ScalingList *scalingList, unsigned sizeId, unsigned listId);
    // Parse HRD information in VPS or in VUI block of SPS
    void parseHrdParameters(H265HRD *hrd, uint8_t commonInfPresentFlag, uint32_t vps_max_sub_layers);

    // Parse profile tier layers header part in VPS or SPS
    void  parsePTL(H265ProfileTierLevel *rpcPTL, int maxNumSubLayersMinus1);
    // Parse one profile tier layer
    void  parseProfileTier(H265PTL *ptl);

    void ParseSEI(mfxPayload *spl);
};

inline bool DecodeExpGolombOne_H265_1u32s (uint32_t **ppBitStream,
                                                      int32_t *pBitOffset,
                                                      int32_t *pDst,
                                                      int32_t isSigned)
{
    uint32_t code;
    uint32_t info     = 0;
    int32_t length   = 1;            /* for first bit read above*/
    uint32_t thisChunksLength = 0;
    uint32_t sval;

    /* check error(s) */

    /* Fast check for element = 0 */
    GetNBits((*ppBitStream), (*pBitOffset), 1, code)
    if (code)
    {
        *pDst = 0;
        return true;
    }

    GetNBits((*ppBitStream), (*pBitOffset), 8, code);
    length += 8;

    /* find nonzero byte */
    while (code == 0 && 32 > length)
    {
        GetNBits((*ppBitStream), (*pBitOffset), 8, code);
        length += 8;
    }

    /* find leading '1' */
    while ((code & 0x80) == 0 && 32 > thisChunksLength)
    {
        code <<= 1;
        thisChunksLength++;
    }
    length -= 8 - thisChunksLength;

    UngetNBits((*ppBitStream), (*pBitOffset),8 - (thisChunksLength + 1))

    /* skipping very long codes, let's assume what the code is corrupted */
    if (32 <= length || 32 <= thisChunksLength)
    {
        uint32_t dwords;
        length -= (*pBitOffset + 1);
        dwords = length/32;
        length -= (32*dwords);
        *ppBitStream += (dwords + 1);
        *pBitOffset = 31 - length;
        *pDst = 0;
        return false;
    }

    /* Get info portion of codeword */
    if (length)
    {
        GetNBits((*ppBitStream), (*pBitOffset),length, info)
    }

    sval = ((1 << (length)) + (info) - 1);
    if (isSigned)
    {
        if (sval & 1)
            *pDst = (int32_t) ((sval + 1) >> 1);
        else
            *pDst = -((int32_t) (sval >> 1));
    }
    else
        *pDst = (int32_t) sval;

    return true;
}

// Read variable length coded unsigned element
inline uint32_t HEVCBaseBitstream::GetVLCElementU()
{
    int32_t sval = 0;

    bool res = DecodeExpGolombOne_H265_1u32s(&m_pbs, &m_bitOffset, &sval, false);

    if (!res)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    return (uint32_t)sval;
}

// Read variable length coded signed element
inline int32_t HEVCBaseBitstream::GetVLCElementS()
{
    int32_t sval = 0;

    bool res = DecodeExpGolombOne_H265_1u32s(&m_pbs, &m_bitOffset, &sval, true);

    if (!res)
        throw HEVC_exception(MFX_ERR_UNDEFINED_BEHAVIOR);

    return sval;
}

} // namespace HEVCParser

#endif // __MFX_C2_HEVC_BITSTREAM_H_
