// Copyright (c) 2017-2025 Intel Corporation
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

#include "mfx_c2_enc_minlevel.h"

// Modified from cts/tests/media/common/src/android/mediav2/common/cts/EncoderProfileLevelTestBase.java
static int divUp(int num, int den){
    return (num + den - 1) / den;
}

C2Config::level_t GetMinLevelAVC(int width, int height, int frameRate, int bitrate) {
    struct LevelLimitAVC {
        LevelLimitAVC(C2Config::level_t level, int mbsPerSec, long mbs, int bitrate) {
            mLevel = level;
            mMbsPerSec = mbsPerSec;
            mMbs = mbs;
            mBitrate = bitrate;
        }

        C2Config::level_t mLevel;
        int mMbsPerSec;
        long mMbs;
        int mBitrate;
    };
    LevelLimitAVC limitsAVC[] = {
            LevelLimitAVC(LEVEL_AVC_1, 1485, 99, 64000),
            LevelLimitAVC(LEVEL_AVC_1B, 1485, 99, 128000),
            LevelLimitAVC(LEVEL_AVC_1_1, 3000, 396, 192000),
            LevelLimitAVC(LEVEL_AVC_1_2, 6000, 396, 384000),
            LevelLimitAVC(LEVEL_AVC_1_3, 11880, 396, 768000),
            LevelLimitAVC(LEVEL_AVC_2, 11880, 396, 2000000),
            LevelLimitAVC(LEVEL_AVC_2_1, 19800, 792, 4000000),
            LevelLimitAVC(LEVEL_AVC_2_2, 20250, 1620, 4000000),
            LevelLimitAVC(LEVEL_AVC_3, 40500, 1620, 10000000),
            LevelLimitAVC(LEVEL_AVC_3_1, 108000, 3600, 14000000),
            LevelLimitAVC(LEVEL_AVC_3_2, 216000, 5120, 20000000),
            LevelLimitAVC(LEVEL_AVC_4, 245760, 8192, 20000000),
            LevelLimitAVC(LEVEL_AVC_4_1, 245760, 8192, 50000000),
            LevelLimitAVC(LEVEL_AVC_4_2, 522240, 8704, 50000000),
            LevelLimitAVC(LEVEL_AVC_5, 589824, 22080, 135000000),
            LevelLimitAVC(LEVEL_AVC_5_1, 983040, 36864, 240000000),
            LevelLimitAVC(LEVEL_AVC_5_2, 2073600, 36864, 240000000),
            // Comment out unsupported levels
            // LevelLimitAVC(LEVEL_AVC_6, 4177920, 139264, 240000000),
            // LevelLimitAVC(LEVEL_AVC_6_1, 8355840, 139264, 480000000),
            // LevelLimitAVC(LEVEL_AVC_6_2), 16711680, 139264, 800000000),
    };
    int blockSize = 16;
    int mbs = divUp(width, blockSize) * divUp(height, blockSize);
    float mbsPerSec = mbs * frameRate;
    for (LevelLimitAVC levelLimitsAVC : limitsAVC) {
        if (mbs <= levelLimitsAVC.mMbs && mbsPerSec <= levelLimitsAVC.mMbsPerSec && bitrate <= levelLimitsAVC.mBitrate) {
            return levelLimitsAVC.mLevel;
        }
    }
    // if none of the levels suffice, select the highest level
    return LEVEL_AVC_5_2;
}

C2Config::level_t GetMinLevelHEVC(int width, int height, int frameRate, int bitrate) {
    struct LevelLimitHEVC {
        LevelLimitHEVC(C2Config::level_t level, long pixelsPerSec, long pixelsPerFrame, int bitrate) {
            mLevel = level;
            mPixelsPerSec = pixelsPerSec;
            mPixelsPerFrame = pixelsPerFrame;
            mBitrate = bitrate;
        }

        C2Config::level_t mLevel;
        long mPixelsPerSec;
        long mPixelsPerFrame;
        int mBitrate;
    };
    LevelLimitHEVC limitsHEVC[] = {
            LevelLimitHEVC(LEVEL_HEVC_MAIN_1, 552960, 36864, 128000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_2, 3686400, 122880, 1500000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_2_1, 7372800, 245760, 3000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_3, 16588800, 552960, 6000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_3_1, 33177600, 983040, 10000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_4, 66846720, 2228224, 12000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_4, 66846720, 2228224, 30000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_4_1, 133693440, 2228224, 20000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_4_1, 133693440, 2228224, 50000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_5, 267386880, 8912896, 25000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_5, 267386880, 8912896, 100000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_5_1, 534773760, 8912896, 40000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_5_1, 534773760, 8912896, 160000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_5_2, 1069547520, 8912896, 60000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_5_2, 1069547520, 8912896, 240000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_6, 1069547520, 35651584, 60000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_6, 1069547520, 35651584, 240000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_6_1, 2139095040, 35651584, 120000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_6_1, 2139095040, 35651584, 480000000),
            LevelLimitHEVC(LEVEL_HEVC_MAIN_6_2, 4278190080L, 35651584, 240000000),
            LevelLimitHEVC(LEVEL_HEVC_HIGH_6_2, 4278190080L, 35651584, 800000000),
    };
    int blockSize = 8;
    int blocks = divUp(width, blockSize) * divUp(height, blockSize);
    long pixelsPerFrame = blocks * blockSize * blockSize;
    long pixelsPerSec = pixelsPerFrame * frameRate;
    for (LevelLimitHEVC levelLimitsHEVC : limitsHEVC) {
        if (pixelsPerFrame <= levelLimitsHEVC.mPixelsPerFrame && pixelsPerSec <= levelLimitsHEVC.mPixelsPerSec && bitrate <= levelLimitsHEVC.mBitrate) {
                return levelLimitsHEVC.mLevel;
        }
    }
    // if none of the levels suffice, select the highest level
    return LEVEL_HEVC_HIGH_6_2;
}

C2Config::level_t GetMinLevelVP9(int width, int height, int frameRate, int bitrate) {
    struct LevelLimitVP9 {
        LevelLimitVP9(C2Config::level_t level, long pixelsPerSec, int size, int maxWH, int bitrate) {
            mLevel = level;
            mPixelsPerSec = pixelsPerSec;
            mSize = size;
            mMaxWH = maxWH;
            mBitrate = bitrate;
        }

        C2Config::level_t mLevel;
        long mPixelsPerSec;
        int mSize;
        int mMaxWH;
        int mBitrate;
    };
    LevelLimitVP9 limitsVP9[] = {
            LevelLimitVP9(LEVEL_VP9_1, 829440, 36864, 512, 200000),
            LevelLimitVP9(LEVEL_VP9_1_1, 2764800, 73728, 768, 800000),
            LevelLimitVP9(LEVEL_VP9_2, 4608000, 122880, 960, 1800000),
            LevelLimitVP9(LEVEL_VP9_2_1, 9216000, 245760, 1344, 3600000),
            LevelLimitVP9(LEVEL_VP9_3, 20736000, 552960, 2048, 7200000),
            LevelLimitVP9(LEVEL_VP9_3_1, 36864000, 983040, 2752, 12000000),
            LevelLimitVP9(LEVEL_VP9_4, 83558400, 2228224, 4160, 18000000),
            LevelLimitVP9(LEVEL_VP9_4_1, 160432128, 2228224, 4160, 30000000),
            LevelLimitVP9(LEVEL_VP9_5, 311951360, 8912896, 8384, 60000000),
            LevelLimitVP9(LEVEL_VP9_5_1, 588251136, 8912896, 8384, 120000000),
            // Comment out unsupported levels
            // LevelLimitVP9(LEVEL_VP9_5_2, 1176502272, 8912896, 8384, 180000000),
            // LevelLimitVP9(LEVEL_VP9_6, 1176502272, 35651584, 16832, 180000000),
            // LevelLimitVP9(LEVEL_VP9_6_1, 2353004544L, 35651584, 16832, 240000000),
            // LevelLimitVP9(LEVEL_VP9_6_2, 4706009088L, 35651584, 16832, 480000000),
    };
    int size = width * height;
    int pixelsPerSec = size * frameRate;
        int maxWH = std::max(width, height);
        for (LevelLimitVP9 levelLimitsVP9 : limitsVP9) {
            if (pixelsPerSec <= levelLimitsVP9.mPixelsPerSec && size <= levelLimitsVP9.mSize && maxWH <= levelLimitsVP9.mMaxWH && bitrate <= levelLimitsVP9.mBitrate) {
                return levelLimitsVP9.mLevel;
            }
        }
        // if none of the levels suffice, select the highest level
        return LEVEL_VP9_5_1;
}


C2Config::level_t GetMinLevelAV1(int width, int height, int frameRate, int bitrate) {
    struct LevelLimitAV1 {
        LevelLimitAV1(C2Config::level_t level, int size, int width, int height, long pixelsPerSec, int bitrate) {
            mLevel = level;
            mSize = size;
            mWidth = width;
            mHeight = height;
            mPixelsPerSec = pixelsPerSec;
            mBitrate = bitrate;
        }

        C2Config::level_t mLevel;
        int mSize;
        int mWidth;
        int mHeight;
        long mPixelsPerSec;
        int mBitrate;
    };
    // taking bitrate from main profile, will also be supported by high profile
    LevelLimitAV1 limitsAV1[] = {
            LevelLimitAV1(LEVEL_AV1_2, 147456, 2048, 1152, 4423680, 1500000),
            LevelLimitAV1(LEVEL_AV1_2_1, 278784, 2816, 1584, 8363520, 3000000),
            LevelLimitAV1(LEVEL_AV1_3, 665856, 4352, 2448, 19975680, 6000000),
            LevelLimitAV1(LEVEL_AV1_3_1, 1065024, 5504, 3096, 31950720, 10000000),
            LevelLimitAV1(LEVEL_AV1_4, 2359296, 6144, 3456, 70778880, 30000000),
            LevelLimitAV1(LEVEL_AV1_4_1, 2359296, 6144, 3456, 141557760, 50000000),
            LevelLimitAV1(LEVEL_AV1_5, 8912896, 8192, 4352, 267386880, 100000000),
            LevelLimitAV1(LEVEL_AV1_5_1, 8912896, 8192, 4352, 534773760, 160000000),
            LevelLimitAV1(LEVEL_AV1_5_2, 8912896, 8192, 4352, 1069547520, 240000000),
            LevelLimitAV1(LEVEL_AV1_5_3, 8912896, 8192, 4352, 1069547520, 240000000),
            LevelLimitAV1(LEVEL_AV1_6, 35651584, 16384, 8704, 1069547520, 240000000),
            LevelLimitAV1(LEVEL_AV1_6_1, 35651584, 16384, 8704, 2139095040, 480000000),
            LevelLimitAV1(LEVEL_AV1_6_2, 35651584, 16384, 8704, 4278190080L, 800000000),
            LevelLimitAV1(LEVEL_AV1_6_3, 35651584, 16384, 8704, 4278190080L, 800000000),
    };
    int size = width * height;
    long pixelsPerSec = (long) size * frameRate;
    for (LevelLimitAV1 levelLimitsAV1 : limitsAV1) {
        if (size <= levelLimitsAV1.mSize && width <= levelLimitsAV1.mWidth && height <= levelLimitsAV1.mHeight
            && pixelsPerSec <= levelLimitsAV1.mPixelsPerSec && bitrate <= levelLimitsAV1.mBitrate) {
            return levelLimitsAV1.mLevel;
        }
    }
    // if none of the levels suffice or high profile, select the highest level
    return LEVEL_AV1_6_3;
}