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

#ifndef __MFX_C2_COLOR_ASPECTS_WRAPPER_H__
#define __MFX_C2_COLOR_ASPECTS_WRAPPER_H__

#include "mfx_c2_utils.h"
#include <media/stagefright/foundation/ColorUtils.h>

class MfxC2ColorAspectsWrapper
{
public:

    MfxC2ColorAspectsWrapper();
    ~MfxC2ColorAspectsWrapper();

    void SetFrameworkColorAspects(const android::ColorAspects &colorAspects);
    void UpdateBitstreamColorAspects(const mfxExtVideoSignalInfo &signalInfo);
    void GetOutputColorAspects(android::ColorAspects &outColorAspects);
    void GetColorAspectsFromVideoSignal(const mfxExtVideoSignalInfo &signalInfo, android::ColorAspects &outColorAspects);

    bool IsColorAspectsChanged();
    void SignalChangedColorAspectsIsSent();

    void SetCodecID(mfxU32 codecId);

private:
    // mfx to C2
    // converters VideoSignalInfo from MFX to C2 API
    void MfxToC2VideoRange(mfxU16 videoRange, android::ColorAspects::Range &out);
    void MfxToC2ColourPrimaries(mfxU16 colourPrimaries, android::ColorAspects::Primaries &out);
    void MfxToC2TransferCharacteristics(mfxU16 transferCharacteristics, android::ColorAspects::Transfer &out);
    void MfxToC2MatrixCoefficients(mfxU16 MatrixCoefficients, android::ColorAspects::MatrixCoeffs &out);

    // C2 to mfx
    // converters VideoSignalInfo from C2 API to MFX
    void C2ToMfxVideoRange(android::ColorAspects::Range videoRange, mfxU16 &out);
    void C2ToMfxColourPrimaries(android::ColorAspects::Primaries colourPrimaries, mfxU16 &out);
    void C2ToMfxTransferCharacteristics(android::ColorAspects::Transfer transferCharacteristics, mfxU16 &out);
    void C2ToMfxMatrixCoefficients(android::ColorAspects::MatrixCoeffs matrixCoefficients, mfxU16 &out);

private:

    // Color aspects passed from the framework.
    android::ColorAspects m_frameworkColorAspects;
    // Color aspects parsed from the bitstream. For decoder use only.
    android::ColorAspects m_bitstreamColorAspects;

    bool m_bIsColorAspectsChanged;

    mfxU32 m_uCodecId;

    MFX_CLASS_NO_COPY(MfxC2ColorAspectsWrapper)
};

#endif // __MFX_C2_COLOR_ASPECTS_WRAPPER_H__
