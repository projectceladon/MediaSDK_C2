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

#include "mfx_c2_color_aspects_wrapper.h"

MfxC2ColorAspectsWrapper::MfxC2ColorAspectsWrapper()
    : bIs_color_aspects_changed_(false)
{
    MFX_DEBUG_TRACE_FUNC;

    codec_ID_ = MFX_CODEC_AVC;

    // Init all the color aspects to be Unspecified.
    memset(&framework_color_aspects_, 0, sizeof(android::ColorAspects));
    memset(&bitstream_color_aspects_, 0, sizeof(android::ColorAspects));
}

MfxC2ColorAspectsWrapper::~MfxC2ColorAspectsWrapper()
{
    MFX_DEBUG_TRACE_FUNC;
}

void MfxC2ColorAspectsWrapper::SetCodecID(mfxU32 codecId)
{
    MFX_DEBUG_TRACE_FUNC;

    codec_ID_ = codecId;
    MFX_DEBUG_TRACE_I32(codec_ID_);
}

void MfxC2ColorAspectsWrapper::SetFrameworkColorAspects(const android::ColorAspects &colorAspects)
{
    MFX_DEBUG_TRACE_FUNC;

    framework_color_aspects_ = colorAspects;

    MFX_DEBUG_TRACE_I32(framework_color_aspects_.mRange);
    MFX_DEBUG_TRACE_I32(framework_color_aspects_.mPrimaries);
    MFX_DEBUG_TRACE_I32(framework_color_aspects_.mTransfer);
    MFX_DEBUG_TRACE_I32(framework_color_aspects_.mMatrixCoeffs);
}

void MfxC2ColorAspectsWrapper::UpdateBitstreamColorAspects(const mfxExtVideoSignalInfo &signalInfo)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_I32(signalInfo.VideoFullRange);
    MFX_DEBUG_TRACE_I32(signalInfo.ColourPrimaries);
    MFX_DEBUG_TRACE_I32(signalInfo.TransferCharacteristics);
    MFX_DEBUG_TRACE_I32(signalInfo.MatrixCoefficients);

    MfxToC2VideoRange(signalInfo.VideoFullRange, bitstream_color_aspects_.mRange);
    MfxToC2ColourPrimaries(signalInfo.ColourPrimaries, bitstream_color_aspects_.mPrimaries);
    MfxToC2TransferCharacteristics(signalInfo.TransferCharacteristics, bitstream_color_aspects_.mTransfer);
    MfxToC2MatrixCoefficients(signalInfo.MatrixCoefficients, bitstream_color_aspects_.mMatrixCoeffs);

    mfxU16 video_signal_type_present_flag = signalInfo.VideoFormat != 5 ||
                                            signalInfo.VideoFullRange != 0 ||
                                            signalInfo.ColourDescriptionPresent != 0;

    if (MFX_CODEC_VP9 == codec_ID_ || MFX_CODEC_VP8 == codec_ID_)
    {
        video_signal_type_present_flag = false;
    }

    if (!video_signal_type_present_flag)
    {
        bitstream_color_aspects_.mRange = android::ColorAspects::RangeUnspecified;
        bitstream_color_aspects_.mPrimaries = android::ColorAspects::PrimariesUnspecified;
        bitstream_color_aspects_.mTransfer  = android::ColorAspects::TransferUnspecified;
        bitstream_color_aspects_.mMatrixCoeffs = android::ColorAspects::MatrixUnspecified;
    }

    if ((bitstream_color_aspects_.mRange        != android::ColorAspects::RangeUnspecified &&
         bitstream_color_aspects_.mRange        != framework_color_aspects_.mRange)     ||
        (bitstream_color_aspects_.mPrimaries    != android::ColorAspects::PrimariesUnspecified &&
         bitstream_color_aspects_.mPrimaries    != framework_color_aspects_.mPrimaries) ||
        (bitstream_color_aspects_.mTransfer     != android::ColorAspects::TransferUnspecified &&
         bitstream_color_aspects_.mTransfer     != framework_color_aspects_.mTransfer)  ||
        (bitstream_color_aspects_.mMatrixCoeffs != android::ColorAspects::MatrixUnspecified &&
         bitstream_color_aspects_.mMatrixCoeffs != framework_color_aspects_.mMatrixCoeffs)
        )
    {
        bIs_color_aspects_changed_ = true;
    }

    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mRange);
    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mPrimaries);
    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mTransfer);
    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mMatrixCoeffs);
}

void MfxC2ColorAspectsWrapper::GetOutputColorAspects(android::ColorAspects &outColorAspects)
{
    MFX_DEBUG_TRACE_FUNC;

    // The component SHALL return the final color aspects
    // by replacing Unspecified coded values with the default values
    // (default values == sent from framework)

    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mRange);
    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mPrimaries);
    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mTransfer);
    MFX_DEBUG_TRACE_I32(bitstream_color_aspects_.mMatrixCoeffs);

    if (bitstream_color_aspects_.mRange != android::ColorAspects::RangeUnspecified)
        outColorAspects.mRange = bitstream_color_aspects_.mRange;
    else
        outColorAspects.mRange = framework_color_aspects_.mRange;

    if (bitstream_color_aspects_.mPrimaries != android::ColorAspects::PrimariesUnspecified)
        outColorAspects.mPrimaries = bitstream_color_aspects_.mPrimaries;
    else
        outColorAspects.mPrimaries = framework_color_aspects_.mPrimaries;

    if (bitstream_color_aspects_.mTransfer != android::ColorAspects::TransferUnspecified)
        outColorAspects.mTransfer = bitstream_color_aspects_.mTransfer;
    else
        outColorAspects.mTransfer = framework_color_aspects_.mTransfer;

    if (bitstream_color_aspects_.mMatrixCoeffs != android::ColorAspects::MatrixUnspecified)
        outColorAspects.mMatrixCoeffs = bitstream_color_aspects_.mMatrixCoeffs;
    else
        outColorAspects.mMatrixCoeffs = framework_color_aspects_.mMatrixCoeffs;

    MFX_DEBUG_TRACE_I32(outColorAspects.mRange);
    MFX_DEBUG_TRACE_I32(outColorAspects.mPrimaries);
    MFX_DEBUG_TRACE_I32(outColorAspects.mTransfer);
    MFX_DEBUG_TRACE_I32(outColorAspects.mMatrixCoeffs);
}

void MfxC2ColorAspectsWrapper::GetColorAspectsFromVideoSignal(const mfxExtVideoSignalInfo &signalInfo, android::ColorAspects &outColorAspects)
{
    MFX_DEBUG_TRACE_FUNC;
    bool video_signal_type_present_flag = signalInfo.VideoFormat != 5 ||
                                            signalInfo.VideoFullRange != 0 ||
                                            signalInfo.ColourDescriptionPresent != 0;

    if (MFX_CODEC_VP9 == codec_ID_ || MFX_CODEC_VP8 == codec_ID_)
    {
        // No video signal info present in vpx bitstream.
        video_signal_type_present_flag = false;
    }

    if (video_signal_type_present_flag)
    {
        MfxToC2ColourPrimaries(signalInfo.ColourPrimaries, outColorAspects.mPrimaries);
        MfxToC2VideoRange(signalInfo.VideoFullRange, outColorAspects.mRange);
        MfxToC2TransferCharacteristics(signalInfo.TransferCharacteristics, outColorAspects.mTransfer);
        MfxToC2MatrixCoefficients(signalInfo.MatrixCoefficients, outColorAspects.mMatrixCoeffs);
    }
    else
    {
        outColorAspects.mPrimaries = android::ColorAspects::PrimariesUnspecified;
        outColorAspects.mRange = android::ColorAspects::RangeUnspecified;
        outColorAspects.mTransfer = android::ColorAspects::TransferUnspecified;
        outColorAspects.mMatrixCoeffs = android::ColorAspects::MatrixUnspecified;
    }

    MFX_DEBUG_TRACE_I32(outColorAspects.mRange);
    MFX_DEBUG_TRACE_I32(outColorAspects.mPrimaries);
    MFX_DEBUG_TRACE_I32(outColorAspects.mTransfer);
    MFX_DEBUG_TRACE_I32(outColorAspects.mMatrixCoeffs);
}

bool MfxC2ColorAspectsWrapper::IsColorAspectsChanged()
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I32(bIs_color_aspects_changed_);

    return bIs_color_aspects_changed_;
}

void MfxC2ColorAspectsWrapper::SignalChangedColorAspectsIsSent()
{
    MFX_DEBUG_TRACE_FUNC;

    bIs_color_aspects_changed_ = false;
}

void MfxC2ColorAspectsWrapper::MfxToC2VideoRange(mfxU16 videoRange, android::ColorAspects::Range &out)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I32(videoRange);

    switch (videoRange)
    {
        case 0:
            out = android::ColorAspects::RangeLimited;
            break;

        case 1:
            out = android::ColorAspects::RangeFull;
            break;

        default:
            out = android::ColorAspects::RangeUnspecified;
            break;
    }

    MFX_DEBUG_TRACE_I32(out);
}

void MfxC2ColorAspectsWrapper::MfxToC2ColourPrimaries(mfxU16 colourPrimaries, android::ColorAspects::Primaries &out)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I32(colourPrimaries);

    switch (colourPrimaries)
    {
        case 1:
            out = android::ColorAspects::PrimariesBT709_5;
            break;

        case 2:
            out = android::ColorAspects::PrimariesUnspecified;
            break;

        case 4:
            out = android::ColorAspects::PrimariesBT470_6M;
            break;

        case 5:
            out = android::ColorAspects::PrimariesBT601_6_625;
            break;

        case 6:
            out = android::ColorAspects::PrimariesBT601_6_525;
            break;

        case 8:
            out = android::ColorAspects::PrimariesGenericFilm;
            break;

        case 9:
            out = android::ColorAspects::PrimariesBT2020;
            break;

        default:
            out = android::ColorAspects::PrimariesUnspecified;
            break;
    }

    MFX_DEBUG_TRACE_I32(out);
}

void MfxC2ColorAspectsWrapper::MfxToC2TransferCharacteristics(mfxU16 transferCharacteristics, android::ColorAspects::Transfer &out)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I32(transferCharacteristics);

    switch (transferCharacteristics)
    {
        case 1:
            out = android::ColorAspects::TransferSMPTE170M;
            break;

        case 2:
            out = android::ColorAspects::TransferUnspecified;
            break;

        case 4:
            out = android::ColorAspects::TransferGamma22;
            break;

        case 5:
            out = android::ColorAspects::TransferGamma28;
            break;

        case 6:
            out = android::ColorAspects::TransferSMPTE170M;
            break;

        case 7:
            out = android::ColorAspects::TransferSMPTE240M;
            break;

        case 8:
            out = android::ColorAspects::TransferLinear;
            break;

        case 11:
            out = android::ColorAspects::TransferXvYCC;
            break;

        case 12:
            out = android::ColorAspects::TransferBT1361;
            break;

        case 13:
            out = android::ColorAspects::TransferSRGB;
            break;

        case 14:
        case 15:
            out = android::ColorAspects::TransferSMPTE170M;
            break;

        case 16:
            out = android::ColorAspects::TransferST2084;
            break;

        case 17:
            out = android::ColorAspects::TransferST428;
            break;

        case 18:
            out = android::ColorAspects::TransferHLG;
            break;

        default:
            out = android::ColorAspects::TransferUnspecified;
            break;
    }

    MFX_DEBUG_TRACE_I32(out);
}

void MfxC2ColorAspectsWrapper::MfxToC2MatrixCoefficients(mfxU16 matrixCoefficients, android::ColorAspects::MatrixCoeffs &out)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I32(matrixCoefficients);

    switch (matrixCoefficients)
    {
        case 1:
            out = android::ColorAspects::MatrixBT709_5;
            break;

        case 2:
            out = android::ColorAspects::MatrixUnspecified;
            break;

        case 4:
            out = android::ColorAspects::MatrixBT470_6M;
            break;

        case 5:
        case 6:
            out = android::ColorAspects::MatrixBT601_6;
            break;

        case 7:
            out = android::ColorAspects::MatrixSMPTE240M;
            break;

        case 9:
            out = android::ColorAspects::MatrixBT2020;
            break;

        case 10:
            out = android::ColorAspects::MatrixBT2020Constant;
            break;

        default:
            out = android::ColorAspects::MatrixUnspecified;
            break;
    }

    MFX_DEBUG_TRACE_I32(out);
}

void MfxC2ColorAspectsWrapper::C2ToMfxVideoRange(android::ColorAspects::Range videoRange, mfxU16 &out)
{
    MFX_DEBUG_TRACE_FUNC;

    switch (videoRange)
    {
        case android::ColorAspects::RangeLimited:
            out = 0;
            break;

        case android::ColorAspects::RangeFull:
            out = 1;
            break;

        case android::ColorAspects::RangeUnspecified:
            out = 0xFF;
            break;

        default:
            // should never happen there
            out = 0;
            ALOGE("Unsupported video range");
            break;
    }
}

void MfxC2ColorAspectsWrapper::C2ToMfxColourPrimaries(android::ColorAspects::Primaries colourPrimaries, mfxU16 &out)
{
    MFX_DEBUG_TRACE_FUNC;

    switch (colourPrimaries)
    {
        case android::ColorAspects::PrimariesBT709_5:
            out = 1;
            break;

        case android::ColorAspects::PrimariesUnspecified:
            out = 2;
            break;

        case android::ColorAspects::PrimariesBT470_6M:
            out = 4;
            break;

        case android::ColorAspects::PrimariesBT601_6_625:
            out = 5;
            break;

        case android::ColorAspects::PrimariesBT601_6_525:
            out = 6;
            break;

        case android::ColorAspects::PrimariesGenericFilm:
            out = 8;
            break;

        case android::ColorAspects::PrimariesBT2020:
            out = 9;
            break;

        default:
            out = 0;
            ALOGE("Unsupported colour primaries");
            break;
    }
}

void MfxC2ColorAspectsWrapper::C2ToMfxTransferCharacteristics(android::ColorAspects::Transfer transferCharacteristics, mfxU16 &out)
{
    MFX_DEBUG_TRACE_FUNC;

    switch (transferCharacteristics)
    {
        case android::ColorAspects::TransferSMPTE170M:
            out = 1;
            break;

        case android::ColorAspects::TransferUnspecified:
            out = 2;
            break;

        case android::ColorAspects::TransferGamma22:
            out = 4;
            break;

        case android::ColorAspects::TransferGamma28:
            out = 5;
            break;

        case android::ColorAspects::TransferSMPTE240M:
            out = 7;
            break;

        case android::ColorAspects::TransferLinear:
            out = 8;
            break;

        case android::ColorAspects::TransferXvYCC:
            out = 11;
            break;

        case android::ColorAspects::TransferBT1361:
            out = 12;
            break;

        case android::ColorAspects::TransferSRGB:
            out = 13;
            break;

        case android::ColorAspects::TransferST2084:
            out = 16;
            break;

        case android::ColorAspects::TransferST428:
            out = 17;
            break;

        case android::ColorAspects::TransferHLG:
            out = 18;
            break;

        default:
            // should never happen there
            out = 0;
            ALOGE("Unsupported transfer characteristic");
            break;
    }
}

void MfxC2ColorAspectsWrapper::C2ToMfxMatrixCoefficients(android::ColorAspects::MatrixCoeffs matrixCoefficients, mfxU16 &out)
{
    MFX_DEBUG_TRACE_FUNC;

    switch (matrixCoefficients)
    {
        case android::ColorAspects::MatrixBT709_5:
            out = 1;
            break;

        case android::ColorAspects::MatrixUnspecified:
            out = 2;
            break;

        case android::ColorAspects::MatrixBT470_6M:
            out = 4;
            break;

        case android::ColorAspects::MatrixBT601_6:
            out = 5;
            break;

        case android::ColorAspects::MatrixSMPTE240M:
            out = 7;
            break;

        case android::ColorAspects::MatrixBT2020:
            out = 9;
            break;

        case android::ColorAspects::MatrixBT2020Constant:
            out = 10;
            break;

        default:
            // should never happen there
            out = 0;
            ALOGE("Unsupported matrix coefficients");
            break;
    }
}
