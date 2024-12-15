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


#include "mfx_frame_constructor.h"
#include "mfx_c2_widevine_crypto_defs.h"

class MfxC2SecureFrameConstructor
{
public:
    MfxC2SecureFrameConstructor();
    virtual ~MfxC2SecureFrameConstructor();
protected:
    virtual mfxStatus Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool header, bool complete_frame);

    // metadata of hucbuffer
    HUCVideoBuffer* m_hucBuffer = nullptr;
    // bs buffer used for WV L1
    std::shared_ptr<mfxBitstream> m_bstEnc;
    // ext buffer vector
    std::vector<mfxExtBuffer*> m_extBufs;
    // MFX_EXTBUFF_ENCRYPTION_PARAM
    mfxExtEncryptionParam m_decryptParams;

private:
    MFX_CLASS_NO_COPY(MfxC2SecureFrameConstructor)
};

class MfxC2AVCSecureFrameConstructor : public MfxC2HEVCFrameConstructor, public MfxC2SecureFrameConstructor
{
public:
    MfxC2AVCSecureFrameConstructor();
    virtual ~MfxC2AVCSecureFrameConstructor();

    virtual mfxStatus Load(const mfxU8* data, mfxU32 size, mfxU64 pts, bool b_header, bool bCompleteFrame);
    virtual mfxStatus Load_data(const mfxU8* data, mfxU32 size, const mfxU8* infobuffer, mfxU64 pts, bool header, bool complete_frame);
    
protected:
    virtual StartCode ReadStartCode(const mfxU8** position, mfxU32* size_left);
    virtual bool   isSPS(mfxI32 code) {return MfxC2AVCFrameConstructor::isSPS(code);}
    virtual bool   isPPS(mfxI32 code) {return MfxC2AVCFrameConstructor::isPPS(code);}
    virtual bool   isIDR(mfxI32 code) {return NAL_UT_AVC_IDR_SLICE == code;}
    virtual bool   isRegularSlice(mfxI32 code) {return NAL_UT_AVC_SLICE == code;}

    std::shared_ptr<mfxBitstream> GetMfxBitstream();

protected:
    bool m_bNeedAttachSPSPPS;

    const static mfxU32 NAL_UT_AVC_SLICE       = 1;
    const static mfxU32 NAL_UT_AVC_IDR_SLICE   = 5;

private:
    MFX_CLASS_NO_COPY(MfxC2AVCSecureFrameConstructor)
};

class MfxC2HEVCSecureFrameConstructor : public MfxC2AVCSecureFrameConstructor
{
public:
    MfxC2HEVCSecureFrameConstructor();
    virtual ~MfxC2HEVCSecureFrameConstructor();

protected:
    virtual bool   isSPS(mfxI32 code) {return MfxC2HEVCFrameConstructor::isSPS(code);}
    virtual bool   isPPS(mfxI32 code) {return MfxC2HEVCFrameConstructor::isPPS(code);}

private:
    MFX_CLASS_NO_COPY(MfxC2HEVCSecureFrameConstructor)
};

class MfxC2SecureFrameConstructorFactory: public MfxC2FrameConstructorFactory
{
public:
    static std::shared_ptr<IMfxC2FrameConstructor> CreateFrameConstructor(MfxC2FrameConstructorType fc_type);
};
