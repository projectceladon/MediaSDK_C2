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

#pragma once

#include "mfx_defs.h"
#ifdef ENABLE_WIDEVINE
#include "mfx_c2_widevine_crypto_defs.h"
#include "mfx_c2_wrap_native_handle.h"
#include "mfx_va_private.h"
#include "mfx_c2_bs_utils.h"
#include "mfx_c2_avc_bitstream.h"
#include "mfx_c2_hevc_bitstream.h"
#endif
#include <memory>
#include <vector>
#include <map>

enum MfxC2FrameConstructorType
{
    MfxC2FC_None,
    MfxC2FC_AVC,
    MfxC2FC_HEVC,
    MfxC2FC_VP8,
    MfxC2FC_VP9,
    MfxC2FC_MP2,
    MfxC2FC_AV1,
    #ifdef ENABLE_WIDEVINE
    MfxC2FC_SEC_AVC,
    MfxC2FC_SEC_HEVC,
    #endif
};

enum MfxC2BitstreamState
{
    MfxC2BS_HeaderAwaiting = 0,
    MfxC2BS_HeaderCollecting = 1,
    MfxC2BS_HeaderWaitSei = 2,
    MfxC2BS_HeaderObtained = 3,
    MfxC2BS_Resetting = 4,
};

class IMfxC2FrameConstructor
{
public:
    virtual ~IMfxC2FrameConstructor() {}

    // inits frame constructor
    virtual mfxStatus Init(mfxU16 profile, mfxFrameInfo fr_info) = 0;
    // loads next portion of data; fc may directly use that buffer or append header, etc.
    virtual mfxStatus Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame) = 0;
    // unloads previously sent buffer, copy data to internal buffer if needed
    virtual mfxStatus Unload() = 0;
    // resets frame constructor
    virtual mfxStatus Reset() = 0;
    // cleans up resources
    virtual void Close() = 0;
    // gets bitstream with data
    virtual std::shared_ptr<mfxBitstream> GetMfxBitstream() = 0;
    // notifies that end of stream reached
    virtual void SetEosMode(bool eos) = 0;
    // returns EOS status
    virtual bool WasEosReached() = 0;
    virtual mfxPayload* GetSEI(mfxU32 /*type*/) = 0;
    // save current SPS/PPS
    virtual mfxStatus SaveHeaders(std::shared_ptr<mfxBitstream> sps, std::shared_ptr<mfxBitstream> pps, bool is_reset) = 0;

protected:
    struct StartCode
    {
        mfxI32 type;
        mfxI32 size;
    };
};

class MfxC2FrameConstructor : public IMfxC2FrameConstructor
{
public:
    MfxC2FrameConstructor();
    virtual ~MfxC2FrameConstructor();

    // inits frame constructor
    virtual mfxStatus Init(mfxU16 profile, mfxFrameInfo fr_info);
    // loads next portion of data; fc may directly use that buffer or append header, etc.
    virtual mfxStatus Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame);
    // unloads previously sent buffer, copy data to internal buffer if needed
    virtual mfxStatus Unload();
    // resets frame constructor
    virtual mfxStatus Reset();
    // cleans up resources
    virtual void Close() { Reset(); }
    // gets bitstream with data
    virtual std::shared_ptr<mfxBitstream> GetMfxBitstream();
    // notifies that end of stream reached
    virtual void SetEosMode(bool eos) { m_bEos = eos; }
    // returns EOS status
    virtual bool WasEosReached() { return m_bEos; }
    // get saved SEI (right now only for HEVC 10 bit SeiHDRStaticInfo)
    virtual mfxPayload* GetSEI(mfxU32 /*type*/) {return nullptr;}
    // save current SPS/PPS
    virtual mfxStatus SaveHeaders(std::shared_ptr<mfxBitstream> sps, std::shared_ptr<mfxBitstream> pps, bool is_reset)
    {
        (void)sps;
        (void)pps;
        (void)is_reset;
        return MFX_ERR_NONE;
    }

protected: // functions
    virtual mfxStatus LoadHeader(const mfxU8* data, mfxU32 size, bool header);
    virtual mfxStatus Load_None(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame);

    // increase buffer capacity with saving of buffer content (realloc)
    mfxStatus BstBufRealloc(mfxU32 add_size);
    // increase buffer capacity without saving of buffer content (free/malloc)
    mfxStatus BstBufMalloc (mfxU32 new_size);
    // cleaning up of internal buffers
    mfxStatus BstBufSync();

protected: // data
    // parameters which define FC behavior
    MfxC2BitstreamState m_bsState;

    // parameters needed for VC1 frame constructor
    mfxFrameInfo m_frInfo;
    mfxU16 m_profile;

    // mfx bistreams:
    // pointer to current bitstream
    std::shared_ptr<mfxBitstream> m_bstCurrent;
    // saved stream header to be returned after seek if no new header will be found
    std::shared_ptr<mfxBitstream> m_bstHeader;
    // buffered data: seq header or remained from previos sample
    std::shared_ptr<mfxBitstream> m_bstBuf;
    // data from input sample (case when buffering and copying is not needed)
    std::shared_ptr<mfxBitstream> m_bstIn;

    // EOS flag
    bool m_bEos;

    // some statistics:
    mfxU32 m_uBstBufReallocs;
    mfxU32 m_uBstBufCopyBytes;

private:
    MFX_CLASS_NO_COPY(MfxC2FrameConstructor)
};

class MfxC2AVCFrameConstructor : public MfxC2FrameConstructor
{
public:
    MfxC2AVCFrameConstructor();
    virtual ~MfxC2AVCFrameConstructor();

    // get saved SEI (right now only for HEVC 10 bit SeiHDRStaticInfo)
    virtual mfxPayload* GetSEI(mfxU32 /*type*/) {return nullptr;}

    // save current SPS/PPS
    virtual mfxStatus SaveHeaders(std::shared_ptr<mfxBitstream> sps, std::shared_ptr<mfxBitstream> pps, bool is_reset);

protected: // functions
    virtual mfxStatus Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame);
    virtual mfxStatus LoadHeader(const mfxU8* data, mfxU32 size, bool header);
    // save current SEI
    virtual mfxStatus SaveSEI(mfxBitstream * /*pSEI*/) {return MFX_ERR_NONE;}

    virtual mfxStatus FindHeaders(const mfxU8* data, mfxU32 size, bool &found_sps, bool &found_pps, bool &found_sei);
    virtual StartCode ReadStartCode(const mfxU8** position, mfxU32& size_left);
    virtual bool      isSPS(mfxI32 code) { return NAL_UT_AVC_SPS == code; }
    virtual bool      isPPS(mfxI32 code) { return NAL_UT_AVC_PPS == code; }
    virtual bool      isSEI(mfxI32 /*code*/) {return false;}
    virtual bool      isIDR(mfxI32 code) {return NAL_UT_AVC_SLICE_IDR == code;}
    virtual bool      needWaitSEI(mfxI32 /*code*/) {return false;}

protected: // data
    const static mfxU32 NAL_UT_AVC_SPS = 7;
    const static mfxU32 NAL_UT_AVC_PPS = 8;
    const static mfxU32 NAL_UT_AVC_SLICE_IDR = 5;

    mfxBitstream m_sps;
    mfxBitstream m_pps;

private:
    MFX_CLASS_NO_COPY(MfxC2AVCFrameConstructor)
};

class MfxC2HEVCFrameConstructor : public MfxC2AVCFrameConstructor
{
public:
    MfxC2HEVCFrameConstructor();
    virtual ~MfxC2HEVCFrameConstructor();

    // get saved SEI (right now only for HEVC 10 bit SeiHDRStaticInfo)
    virtual mfxPayload* GetSEI(mfxU32 type);

    const static mfxU32 SEI_MASTERING_DISPLAY_COLOUR_VOLUME = 137;
    const static mfxU32 SEI_CONTENT_LIGHT_LEVEL_INFO = 144;

protected: // functions
    virtual StartCode ReadStartCode(const mfxU8** position, mfxU32& size_left);
    virtual bool      isSPS(mfxI32 code) { return NAL_UT_HEVC_SPS == code; }
    virtual bool      isPPS(mfxI32 code) { return NAL_UT_HEVC_PPS == code; }
    // save current SEI
    virtual mfxStatus SaveSEI(mfxBitstream *pSEI);
    virtual bool   isSEI(mfxI32 code) {return NAL_UT_HEVC_SEI == code;}
    virtual bool   isIDR(mfxI32 code) {return NAL_UT_HEVC_IDR_W_RADL == code || NAL_UT_HEVC_IDR_N_LP == code;}
    virtual bool   needWaitSEI(mfxI32 code) { return NAL_UT_CODED_SLICEs.end() == std::find(NAL_UT_CODED_SLICEs.begin(), NAL_UT_CODED_SLICEs.end(), code);}

protected: // data
    const static mfxU32 NAL_UT_HEVC_SPS = 33;
    const static mfxU32 NAL_UT_HEVC_PPS = 34;
    const static mfxU32 NAL_UT_HEVC_SEI = 39;
    const static mfxU32 NAL_UT_HEVC_IDR_W_RADL = 19;
    const static mfxU32 NAL_UT_HEVC_IDR_N_LP  = 20;
    const static std::vector<mfxU32> NAL_UT_CODED_SLICEs;

    std::map<mfxU32, mfxPayload> m_SEIMap;

private:
    MFX_CLASS_NO_COPY(MfxC2HEVCFrameConstructor)
};

#ifdef ENABLE_WIDEVINE
class MfxC2SecureFrameConstructor
{
public:
    MfxC2SecureFrameConstructor();
    virtual ~MfxC2SecureFrameConstructor();

    MFX_CLASS_NO_COPY(MfxC2SecureFrameConstructor)

protected:
    virtual mfxStatus Reset(void);
    virtual mfxStatus ResetHeaders(void);
    virtual mfxStatus ConstructFrame(VACencStatusBuf* cencStatus, mfxBitstream* bs);
    virtual mfxStatus Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame);
    virtual mfxEncryptedData* GetFreeEncryptedDataItem(void);
    virtual mfxEncryptedData* BuildEncryptedDataList(void);
    virtual mfxStatus GetSliceHeader(mfxU8* data, mfxU32 size, mfxU8 **sliceHdr, mfxU32 &sliceHdrlength);

    virtual mfxStatus PackSliceHeader(OutputBitstream & obs, mfxU8* data, mfxU32 size) = 0;
    virtual mfxStatus ParseHeaders() = 0;

protected:
    static const mfxU32 SLICE_HEADER_BUFFER_SIZE = 128;
    std::list<mfxEncryptedData*> m_encryptedDataList;
    HUCVideoBuffer               m_hucBuffer;

    // Actual SPS/PPS/SEI headers
    mfxBitstream m_SPS_PPS_SEI;
    // tmp buffer for slice header packing
    std::vector<mfxU8>  m_sliceHeader;
    // m_ClearBst contains clear data - SPS, PPS, SEI, slice header
    mfxBitstream m_ClearBst;
};

class MfxC2AVCSecureFrameConstructor : public MfxC2HEVCFrameConstructor, public MfxC2SecureFrameConstructor
{
public:
    MfxC2AVCSecureFrameConstructor();
    virtual ~MfxC2AVCSecureFrameConstructor(void);
    MFX_CLASS_NO_COPY(MfxC2AVCSecureFrameConstructor)

    virtual mfxStatus Reset(void);
    virtual mfxStatus Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool b_header, bool bCompleteFrame);

protected:
    virtual bool   isSPS(mfxI32 code) {return MfxC2AVCFrameConstructor::isSPS(code);}
    virtual bool   isPPS(mfxI32 code) {return MfxC2AVCFrameConstructor::isPPS(code);}
    virtual bool   isIDR(mfxI32 code) {return NAL_UT_AVC_IDR_SLICE == code;}
    virtual bool   isRegularSlice(mfxI32 code) {return NAL_UT_AVC_SLICE == code;}

    std::shared_ptr<mfxBitstream> GetMfxBitstream();

    virtual mfxStatus ParseHeaders();
    virtual mfxStatus ParseNalUnit(mfxU8 * const data, mfxU32 NAlUnitSize);
    virtual mfxStatus PackSliceHeader(OutputBitstream & obs, mfxU8* data, mfxU32 size);

    mfxU8* GetMemoryForSwapping(mfxU32 size);

protected:
    bool m_bNeedAttachSPSPPS;

    const static mfxU32 NAL_UT_AVC_SLICE       = 1;
    const static mfxU32 NAL_UT_AVC_IDR_SLICE   = 5;

    AVCParser::AVCHeaders m_H264Headers;
    std::vector<mfxU8>  m_swappingMemory;
};

/*------------------------------------------------------------------------------*/

class MfxC2HEVCSecureFrameConstructor : public MfxC2AVCSecureFrameConstructor
{
public:
    MfxC2HEVCSecureFrameConstructor();
    virtual ~MfxC2HEVCSecureFrameConstructor(void);

    MFX_CLASS_NO_COPY(MfxC2HEVCSecureFrameConstructor)
protected:
    virtual StartCode ReadStartCode(const mfxU8** position, mfxU32& size_left);
    virtual bool   isSPS(mfxI32 code) {return MfxC2HEVCFrameConstructor::isSPS(code);}
    virtual bool   isPPS(mfxI32 code) {return MfxC2HEVCFrameConstructor::isPPS(code);}
    virtual bool   isIDR(mfxI32 code) {return (NAL_UT_HEVC_SLICE_IDR_W_RADL == code) || (NAL_UT_HEVC_SLICE_IDR_N_LP == code);}
    virtual bool   isRegularSlice(mfxI32 code);

    virtual mfxStatus ParseNalUnit(mfxU8 * const data, mfxU32 NAlUnitSize);
    virtual mfxStatus PackSliceHeader(OutputBitstream & obs, mfxU8* data, mfxU32 size);

protected:
    const static mfxU32 NAL_UT_HEVC_SLICE_TRAIL_N    = 0;
    const static mfxU32 NAL_UT_HEVC_SLICE_TRAIL_R    = 1;
    const static mfxU32 NAL_UT_HEVC_SLICE_TSA_N      = 2;
    const static mfxU32 NAL_UT_HEVC_SLICE_TLA_R      = 3;
    const static mfxU32 NAL_UT_HEVC_SLICE_STSA_N     = 4;
    const static mfxU32 NAL_UT_HEVC_SLICE_STSA_R     = 5;
    const static mfxU32 NAL_UT_HEVC_SLICE_RADL_N     = 6;
    const static mfxU32 NAL_UT_HEVC_SLICE_RADL_R     = 7;
    const static mfxU32 NAL_UT_HEVC_SLICE_RASL_N     = 8;
    const static mfxU32 NAL_UT_HEVC_SLICE_RASL_R     = 9;
    const static mfxU32 NAL_UT_HEVC_SLICE_BLA_W_LP   = 16;
    const static mfxU32 NAL_UT_HEVC_SLICE_BLA_W_RADL = 17;
    const static mfxU32 NAL_UT_HEVC_SLICE_BLA_N_LP   = 18;
    const static mfxU32 NAL_UT_HEVC_SLICE_IDR_W_RADL = 19;
    const static mfxU32 NAL_UT_HEVC_SLICE_IDR_N_LP   = 20;
    const static mfxU32 NAL_UT_HEVC_SLICE_CRA        = 21;

    HEVCParser::HEVCHeaders m_H265Headers;
    int32_t previous_poc;
};
#endif

class MfxC2FrameConstructorFactory
{
public:
    static std::shared_ptr<IMfxC2FrameConstructor> CreateFrameConstructor(MfxC2FrameConstructorType fc_type);
};
