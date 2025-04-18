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

#include "mfx_c2_decoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"
#include "C2PlatformSupport.h"
#include "mfx_c2_color_aspects_utils.h"

#include <C2AllocatorGralloc.h>
#include <Codec2Mapper.h>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_decoder_component"

constexpr uint32_t MIN_W = 176;
constexpr uint32_t MIN_H = 144;
constexpr c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;
constexpr uint64_t kMinInputBufferSize = 1 * WIDTH_1K * HEIGHT_1K;
constexpr uint64_t kDefaultConsumerUsage =
    (GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_COMPOSER);
constexpr uint64_t kProtectedUsage = C2MemoryUsage::READ_PROTECTED;

// Android S declared VP8 profile
#if PLATFORM_SDK_VERSION <= 30 // Android 11(R)
enum VP8_PROFILE {
    PROFILE_VP8_0 = C2_PROFILE_LEVEL_VENDOR_START,
};
#endif

enum VP8_LEVEL {
    LEVEL_VP8_Version0 = C2_PROFILE_LEVEL_VENDOR_START,
};


C2R MfxC2DecoderComponent::OutputSurfaceAllocatorSetter(bool mayBlock, C2P<C2PortSurfaceAllocatorTuning::output> &me) {
    (void)mayBlock;
    (void)me;
    C2R res = C2R::Ok();

    return res;
}

C2R MfxC2DecoderComponent::SizeSetter(bool mayBlock, const C2P<C2StreamPictureSizeInfo::output> &oldMe,
                                    C2P<C2StreamPictureSizeInfo::output> &me) {
    (void)mayBlock;
    C2R res = C2R::Ok();
    if (!me.F(me.v.width).supportsAtAll(me.v.width)) {
        res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.width)));
        me.set().width = oldMe.v.width;
    }
    if (!me.F(me.v.height).supportsAtAll(me.v.height)) {
        res = res.plus(C2SettingResultBuilder::BadValue(me.F(me.v.height)));
        me.set().height = oldMe.v.height;
    }
    return res;
}

C2R MfxC2DecoderComponent::MaxPictureSizeSetter(bool mayBlock, C2P<C2StreamMaxPictureSizeTuning::output> &me,
                                            const C2P<C2StreamPictureSizeInfo::output> &size) {
    (void)mayBlock;
    // TODO: get max width/height from the size's field helpers vs. hardcoding
    me.set().width = c2_min(c2_max(me.v.width, size.v.width), uint32_t(WIDTH_8K));
    me.set().height = c2_min(c2_max(me.v.height, size.v.height), uint32_t(HEIGHT_8K));
    return C2R::Ok();
}

C2R MfxC2DecoderComponent::MaxInputSizeSetter(bool mayBlock, C2P<C2StreamMaxBufferSizeInfo::input> &me,
                                const C2P<C2StreamMaxPictureSizeTuning::output> &maxSize) {
    (void)mayBlock;
    // assume compression ratio of 2
    me.set().value = c2_max((((maxSize.v.width + 63) / 64) * ((maxSize.v.height + 63) / 64) * 3072),
		   kMinInputBufferSize);
    return C2R::Ok();
}

C2R MfxC2DecoderComponent::ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::input> &me,
                                  const C2P<C2StreamPictureSizeInfo::output> &size) {
    (void)mayBlock;
    (void)size;
    (void)me;  // TODO: validate
    return C2R::Ok();
}

C2R MfxC2DecoderComponent::OutputUsageSetter(bool mayBlock, C2P<C2StreamUsageTuning::output> &me) {
    (void)mayBlock;
    (void)me;
    C2R res = C2R::Ok();
    return res;
}

C2R MfxC2DecoderComponent::DefaultColorAspectsSetter(bool mayBlock,
                                            C2P<C2StreamColorAspectsTuning::output> &me) {
    (void)mayBlock;
    if (me.v.range > C2Color::RANGE_OTHER) {
            me.set().range = C2Color::RANGE_OTHER;
    }
    if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
            me.set().primaries = C2Color::PRIMARIES_OTHER;
    }
    if (me.v.transfer > C2Color::TRANSFER_OTHER) {
            me.set().transfer = C2Color::TRANSFER_OTHER;
    }
    if (me.v.matrix > C2Color::MATRIX_OTHER) {
            me.set().matrix = C2Color::MATRIX_OTHER;
    }
    return C2R::Ok();
}

C2R MfxC2DecoderComponent::CodedColorAspectsSetter(bool mayBlock,
                                            C2P<C2StreamColorAspectsInfo::input> &me) {
    (void)mayBlock;
    if (me.v.range > C2Color::RANGE_OTHER) {
            me.set().range = C2Color::RANGE_OTHER;
    }
    if (me.v.primaries > C2Color::PRIMARIES_OTHER) {
            me.set().primaries = C2Color::PRIMARIES_OTHER;
    }
    if (me.v.transfer > C2Color::TRANSFER_OTHER) {
            me.set().transfer = C2Color::TRANSFER_OTHER;
    }
    if (me.v.matrix > C2Color::MATRIX_OTHER) {
            me.set().matrix = C2Color::MATRIX_OTHER;
    }
    return C2R::Ok();
}

C2R MfxC2DecoderComponent::ColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::output> &me,
                                const C2P<C2StreamColorAspectsTuning::output> &def,
                                const C2P<C2StreamColorAspectsInfo::input> &coded) {
    (void)mayBlock;
    // take default values for all unspecified fields, and coded values for specified ones
    me.set().range = coded.v.range == RANGE_UNSPECIFIED ? def.v.range : coded.v.range;
    me.set().primaries = coded.v.primaries == PRIMARIES_UNSPECIFIED
            ? def.v.primaries : coded.v.primaries;
    me.set().transfer = coded.v.transfer == TRANSFER_UNSPECIFIED
            ? def.v.transfer : coded.v.transfer;
    me.set().matrix = coded.v.matrix == MATRIX_UNSPECIFIED ? def.v.matrix : coded.v.matrix;
    return C2R::Ok();
}

MfxC2DecoderComponent::MfxC2DecoderComponent(const C2String name, const CreateConfig& config,
    std::shared_ptr<C2ReflectorHelper> reflector, DecoderType decoder_type) :
        MfxC2Component(name, config, std::move(reflector)),
        m_decoderType(decoder_type),
#ifdef USE_ONEVPL
        m_mfxSession(nullptr),
        m_mfxLoader(nullptr),
#endif
        m_bInitialized(false),
        m_bInitialized_once(false),
        m_uSyncedPointsCount(0),
        m_bSetHdrStatic(false),
        m_surfaceNum(0),
        m_secure(false)
{
    MFX_DEBUG_TRACE_FUNC;
    const unsigned int SINGLE_STREAM_ID = 0u;

    addParameter(
        DefineParam(m_kind, C2_PARAMKEY_COMPONENT_KIND)
        .withConstValue(new C2ComponentKindSetting(C2Component::KIND_DECODER))
        .build());

    addParameter(
        DefineParam(m_domain, C2_PARAMKEY_COMPONENT_DOMAIN)
        .withConstValue(new C2ComponentDomainSetting(C2Component::DOMAIN_VIDEO))
        .build());

    addParameter(
        DefineParam(m_name, C2_PARAMKEY_COMPONENT_NAME)
        .withConstValue(AllocSharedString<C2ComponentNameSetting>(name.c_str()))
        .build());

    addParameter(
        DefineParam(m_surfaceAllocator, C2_PARAMKEY_OUTPUT_SURFACE_ALLOCATOR)
        .withConstValue(new C2PortSurfaceAllocatorTuning::output(C2PlatformAllocatorStore::BUFFERQUEUE))
        .withSetter(OutputSurfaceAllocatorSetter)
        .build());

    C2Allocator::id_t outputAllocators[1] = { C2PlatformAllocatorStore::GRALLOC };
    addParameter(
        DefineParam(m_outputAllocators, C2_PARAMKEY_OUTPUT_ALLOCATORS)
        .withDefault(C2PortAllocatorsTuning::output::AllocShared(outputAllocators))
        .withFields({ C2F(m_outputAllocators, m.values[0]).any(),
                      C2F(m_outputAllocators, m.values).inRange(0, 1) })
        .withSetter(Setter<C2PortAllocatorsTuning::output>::NonStrictValuesWithNoDeps)
        .build());

    addParameter(
        DefineParam(m_inputFormat, C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE)
        .withConstValue(new C2StreamBufferTypeSetting::input(
                    SINGLE_STREAM_ID, C2BufferData::LINEAR))
        .build());

    addParameter(
        DefineParam(m_outputFormat, C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE)
        .withConstValue(new C2StreamBufferTypeSetting::output(
                    SINGLE_STREAM_ID, C2BufferData::GRAPHIC))
        .build());

    addParameter(
        DefineParam(m_outputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
        .withConstValue(AllocSharedString<C2PortMediaTypeSetting::output>("video/raw"))
        .build());

    m_consumerUsage = C2AndroidMemoryUsage::FromGrallocUsage(kDefaultConsumerUsage).expected;
    addParameter(
        DefineParam(m_outputUsage, C2_PARAMKEY_OUTPUT_STREAM_USAGE)
        .withDefault(new C2StreamUsageTuning::output(SINGLE_STREAM_ID, m_consumerUsage))
        .withFields({ C2F(m_outputUsage, value).any(),})
        .withSetter(OutputUsageSetter)
        .build()
    );

    switch(m_decoderType) {
        case DECODER_H264_SECURE:
        case DECODER_H264: {
            m_uOutputDelay = /*max_dpb_size*/16 + /*for async depth*/1 + /*for msdk unref in sync part*/1;

	    m_uInputDelay = 15;

            addParameter(
                DefineParam(m_inputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>("video/avc"))
                .build());

            addParameter(
                DefineParam(m_size, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(MIN_W, WIDTH_4K, 2),
                    C2F(m_size, height).inRange(MIN_H, HEIGHT_4K, 2),})
                .withSetter(SizeSetter)
                .build());

            addParameter(
                DefineParam(m_maxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(2, WIDTH_4K, 2),
                    C2F(m_size, height).inRange(2, HEIGHT_4K, 2),
                })
                .withSetter(MaxPictureSizeSetter, m_size)
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::input(
                    SINGLE_STREAM_ID, PROFILE_AVC_CONSTRAINED_BASELINE, LEVEL_AVC_5_2))
                .withFields({
                    C2F(m_profileLevel, C2ProfileLevelStruct::profile)
                        .oneOf({
                            PROFILE_AVC_BASELINE,
                            PROFILE_AVC_CONSTRAINED_BASELINE,
                            PROFILE_AVC_MAIN,
                            PROFILE_AVC_CONSTRAINED_HIGH,
                            PROFILE_AVC_PROGRESSIVE_HIGH,
                            PROFILE_AVC_HIGH,
                        }),
                    C2F(m_profileLevel, C2ProfileLevelStruct::level)
                        .oneOf({
                            LEVEL_AVC_1, LEVEL_AVC_1B, LEVEL_AVC_1_1,
                            LEVEL_AVC_1_2, LEVEL_AVC_1_3,
                            LEVEL_AVC_2, LEVEL_AVC_2_1, LEVEL_AVC_2_2,
                            LEVEL_AVC_3, LEVEL_AVC_3_1, LEVEL_AVC_3_2,
                            LEVEL_AVC_4, LEVEL_AVC_4_1, LEVEL_AVC_4_2,
                            LEVEL_AVC_5, LEVEL_AVC_5_1, LEVEL_AVC_5_2,
                        }),})
                .withSetter(ProfileLevelSetter, m_size)
                .build());
            break;
        }
        case DECODER_H265_SECURE:
        case DECODER_H265: {
            m_uOutputDelay = /*max_dpb_size*/16 + /*for async depth*/1 + /*for msdk unref in sync part*/1;

	    m_uInputDelay = 15;

            addParameter(
                DefineParam(m_inputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>("video/hevc"))
                .build());

            addParameter(
                DefineParam(m_size, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(MIN_W, WIDTH_8K, 2),
                    C2F(m_size, height).inRange(MIN_H, HEIGHT_8K, 2),})
                .withSetter(SizeSetter)
                .build());

            addParameter(
                DefineParam(m_maxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(2, WIDTH_8K, 2),
                    C2F(m_size, height).inRange(2, HEIGHT_8K, 2),
                })
                .withSetter(MaxPictureSizeSetter, m_size)
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::input(
                    SINGLE_STREAM_ID, PROFILE_HEVC_MAIN, LEVEL_HEVC_MAIN_5_1))
                .withFields({
                    C2F(m_profileLevel, C2ProfileLevelStruct::profile)
                        .oneOf({
                            PROFILE_HEVC_MAIN,
                            PROFILE_HEVC_MAIN_STILL,
                            PROFILE_HEVC_MAIN_10,
                        }),
                    C2F(m_profileLevel, C2ProfileLevelStruct::level)
                        .oneOf({
                            LEVEL_HEVC_MAIN_1,
                            LEVEL_HEVC_MAIN_2, LEVEL_HEVC_MAIN_2_1,
                            LEVEL_HEVC_MAIN_3, LEVEL_HEVC_MAIN_3_1,
                            LEVEL_HEVC_MAIN_4, LEVEL_HEVC_MAIN_4_1,
                            LEVEL_HEVC_HIGH_4, LEVEL_HEVC_HIGH_4_1,
                            LEVEL_HEVC_MAIN_5, LEVEL_HEVC_MAIN_5_1,
                            LEVEL_HEVC_MAIN_5_2, LEVEL_HEVC_HIGH_5,
                            LEVEL_HEVC_HIGH_5_1, LEVEL_HEVC_HIGH_5_2,
                            LEVEL_HEVC_MAIN_6, LEVEL_HEVC_MAIN_6_1,
                            LEVEL_HEVC_MAIN_6_2, LEVEL_HEVC_HIGH_6,
                            LEVEL_HEVC_HIGH_6_1, LEVEL_HEVC_HIGH_6_2
                        }),})
                .withSetter(ProfileLevelSetter, m_size)
                .build());
            break;
        }
        case DECODER_VP9: {
            m_uOutputDelay = /*max_dpb_size*/9 + /*for async depth*/1 + /*for msdk unref in sync part*/1;

	    m_uInputDelay = 15;

            addParameter(
                DefineParam(m_inputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>("video/x-vnd.on2.vp9"))
                .build());

            addParameter(
                DefineParam(m_size, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(MIN_W, WIDTH_8K, 2),
                    C2F(m_size, height).inRange(MIN_H, HEIGHT_8K, 2),})
                .withSetter(SizeSetter)
                .build());

            addParameter(
                DefineParam(m_maxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(2, WIDTH_8K, 2),
                    C2F(m_size, height).inRange(2, HEIGHT_8K, 2),
                })
                .withSetter(MaxPictureSizeSetter, m_size)
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::input(
                    SINGLE_STREAM_ID, PROFILE_VP9_0, LEVEL_VP9_5))
                .withFields({
                    C2F(m_profileLevel, C2ProfileLevelStruct::profile)
                        .oneOf({
                            PROFILE_VP9_0,
                            PROFILE_VP9_1,
                            PROFILE_VP9_2,
                            PROFILE_VP9_3,
                        }),
                    C2F(m_profileLevel, C2ProfileLevelStruct::level)
                        .oneOf({
                            LEVEL_VP9_1, LEVEL_VP9_1_1,
                            LEVEL_VP9_2, LEVEL_VP9_2_1,
                            LEVEL_VP9_3, LEVEL_VP9_3_1,
                            LEVEL_VP9_4, LEVEL_VP9_4_1,
                            LEVEL_VP9_5, LEVEL_VP9_5_1
                        }),})
                .withSetter(ProfileLevelSetter, m_size)
                .build());
            break;
        }
        case DECODER_VP8: {
            m_uOutputDelay = /*max_dpb_size*/8 + /*for async depth*/1 + /*for msdk unref in sync part*/1;

	    m_uInputDelay = 15;

            addParameter(
                DefineParam(m_inputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>("video/x-vnd.on2.vp8"))
                .build());

            addParameter(
                DefineParam(m_size, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(MIN_W, WIDTH_4K, 2),
                    C2F(m_size, height).inRange(MIN_H, HEIGHT_4K, 2),})
                .withSetter(SizeSetter)
                .build());

            addParameter(
                DefineParam(m_maxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(2, WIDTH_4K, 2),
                    C2F(m_size, height).inRange(2, HEIGHT_4K, 2),
                })
                .withSetter(MaxPictureSizeSetter, m_size)
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::input(
#if PLATFORM_SDK_VERSION <= 30 // Android 11(R)
                    SINGLE_STREAM_ID, PROFILE_VP8_0, C2Config::LEVEL_UNUSED))
                .withFields({
                    C2F(m_profileLevel, profile).equalTo(
                        PROFILE_VP8_0),
#else
                    SINGLE_STREAM_ID, C2Config::profile_t::PROFILE_VP8_0, C2Config::LEVEL_UNUSED))
                .withFields({
                    C2F(m_profileLevel, profile).equalTo(
                        C2Config::profile_t::PROFILE_VP8_0),
#endif
                    C2F(m_profileLevel, level).equalTo(
                        C2Config::LEVEL_UNUSED),
                })
                .withSetter(ProfileLevelSetter, m_size)
                .build());
            break;
        }
        case DECODER_MPEG2: {
            m_uOutputDelay = /*max_dpb_size*/4 + /*for async depth*/1 + /*for msdk unref in sync part*/1;

	    m_uInputDelay = 15;

            addParameter(
                DefineParam(m_inputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>("video/mpeg2"))
                .build());

            addParameter(
                DefineParam(m_size, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(MIN_W, WIDTH_2K, 2),
                    C2F(m_size, height).inRange(MIN_H, HEIGHT_2K, 2),})
                .withSetter(SizeSetter)
                .build());

            addParameter(
                DefineParam(m_maxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(2, WIDTH_2K, 2),
                    C2F(m_size, height).inRange(2, HEIGHT_2K, 2),
                })
                .withSetter(MaxPictureSizeSetter, m_size)
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::input(
                    SINGLE_STREAM_ID, PROFILE_MP2V_MAIN, LEVEL_MP2V_MAIN))
                .withFields({
                    C2F(m_profileLevel, C2ProfileLevelStruct::profile)
                        .oneOf({
                            PROFILE_MP2V_SIMPLE,
                            PROFILE_MP2V_MAIN,
                        }),
                    C2F(m_profileLevel, C2ProfileLevelStruct::level)
                        .oneOf({
                            LEVEL_MP2V_LOW, LEVEL_MP2V_MAIN,
                            LEVEL_MP2V_HIGH_1440,
                        }),})
                .withSetter(ProfileLevelSetter, m_size)
                .build());
            break;
        }
        case DECODER_AV1: {
            m_uOutputDelay = /*max_dpb_size*/18 + /*for async depth*/1 + /*for msdk unref in sync part*/1;

	    m_uInputDelay = 15;

            addParameter(
                DefineParam(m_inputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
                .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>("video/av01"))
                .build());

            addParameter(
                DefineParam(m_size, C2_PARAMKEY_PICTURE_SIZE)
                .withDefault(new C2StreamPictureSizeInfo::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(MIN_W, WIDTH_8K, 2),
                    C2F(m_size, height).inRange(MIN_H, HEIGHT_8K, 2),})
                .withSetter(SizeSetter)
                .build());

            addParameter(
                DefineParam(m_maxSize, C2_PARAMKEY_MAX_PICTURE_SIZE)
                .withDefault(new C2StreamMaxPictureSizeTuning::output(SINGLE_STREAM_ID, MIN_W, MIN_H))
                .withFields({
                    C2F(m_size, width).inRange(2, WIDTH_8K, 2),
                    C2F(m_size, height).inRange(2, HEIGHT_8K, 2),
                })
                .withSetter(MaxPictureSizeSetter, m_size)
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::input(
                    SINGLE_STREAM_ID, PROFILE_AV1_0, LEVEL_AV1_2))
                .withFields({
                    C2F(m_profileLevel, C2ProfileLevelStruct::profile)
                        .oneOf({
                            PROFILE_AV1_0,
                            PROFILE_AV1_1,
                        }),
                    C2F(m_profileLevel, C2ProfileLevelStruct::level)
                        .oneOf({
                            LEVEL_AV1_2, LEVEL_AV1_2_1,
                            LEVEL_AV1_2_2, LEVEL_AV1_2_3,
                            LEVEL_AV1_3, LEVEL_AV1_3_1,
                            LEVEL_AV1_3_2, LEVEL_AV1_3_3,
                            LEVEL_AV1_4, LEVEL_AV1_4_1,
                            LEVEL_AV1_4_2, LEVEL_AV1_4_3,
                            LEVEL_AV1_5, LEVEL_AV1_5_1,
                            LEVEL_AV1_5_2, LEVEL_AV1_5_3,
                            LEVEL_AV1_6, LEVEL_AV1_6_1,
                            LEVEL_AV1_6_2, LEVEL_AV1_6_3
                        }),})
                .withSetter(ProfileLevelSetter, m_size)
                .build());
            break;
        }
        default:
        break;
    }

    addParameter(
        DefineParam(m_maxInputSize, C2_PARAMKEY_INPUT_MAX_BUFFER_SIZE)
        .withDefault(new C2StreamMaxBufferSizeInfo::input(SINGLE_STREAM_ID, kMinInputBufferSize))
        .withFields({
            C2F(m_maxInputSize, value).any(),
        })
        .calculatedAs(MaxInputSizeSetter, m_maxSize)
        .build());

    C2BlockPool::local_id_t outputPoolIds[1] = { C2BlockPool::PLATFORM_START };
    addParameter(
        DefineParam(m_outputPoolIds, C2_PARAMKEY_OUTPUT_BLOCK_POOLS)
        .withDefault(C2PortBlockPoolsTuning::output::AllocShared(outputPoolIds))
        .withFields({ C2F(m_outputPoolIds, m.values[0]).any(),
                        C2F(m_outputPoolIds, m.values).inRange(0, 1) })
        .withSetter(Setter<C2PortBlockPoolsTuning::output>::NonStrictValuesWithNoDeps)
        .build());

    // C2PortDelayTuning::output parameter is needed to say framework about the max delay expressed in
    // decoded frames. If parameter is set too low, framework will stop sanding new portions
    // of bitstream and will wait for decoded frames.
    // The parameter value is differet for codecs and must be equal the DPD value is gotten
    // form QueryIOSurf function call result.
    addParameter(
        DefineParam(m_actualOutputDelay, C2_PARAMKEY_OUTPUT_DELAY)
        .withDefault(new C2PortActualDelayTuning::output(m_uOutputDelay))
        .withFields({C2F(m_actualOutputDelay, value).inRange(0, m_uOutputDelay)})
        .withSetter(Setter<decltype(*m_actualOutputDelay)>::StrictValueWithNoDeps)
        .build());

    addParameter(
        DefineParam(m_inputDelay, C2_PARAMKEY_INPUT_DELAY)
        .withConstValue(new C2PortDelayTuning::input(m_uInputDelay))
        .build());

    addParameter(
        DefineParam(m_defaultColorAspects, C2_PARAMKEY_DEFAULT_COLOR_ASPECTS)
        .withDefault(new C2StreamColorAspectsTuning::output(
                SINGLE_STREAM_ID, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
        .withFields({
            C2F(m_defaultColorAspects, range).inRange(
                        C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
            C2F(m_defaultColorAspects, primaries).inRange(
                        C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
            C2F(m_defaultColorAspects, transfer).inRange(
                        C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
            C2F(m_defaultColorAspects, matrix).inRange(
                        C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
        })
        .withSetter(DefaultColorAspectsSetter)
        .build());

    if (DECODER_VP8 != m_decoderType) {
        addParameter(
            DefineParam(m_inColorAspects, C2_PARAMKEY_VUI_COLOR_ASPECTS)
            .withDefault(new C2StreamColorAspectsInfo::input(
                    SINGLE_STREAM_ID, C2Color::RANGE_LIMITED, C2Color::PRIMARIES_UNSPECIFIED,
                    C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
            .withFields({
                C2F(m_inColorAspects, range).inRange(
                            C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                C2F(m_inColorAspects, primaries).inRange(
                            C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                C2F(m_inColorAspects, transfer).inRange(
                            C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                C2F(m_inColorAspects, matrix).inRange(
                            C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
            })
            .withSetter(CodedColorAspectsSetter)
            .build());

        addParameter(
            DefineParam(m_outColorAspects, C2_PARAMKEY_COLOR_ASPECTS)
            .withDefault(new C2StreamColorAspectsInfo::output(
                    SINGLE_STREAM_ID, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                    C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
            .withFields({
                C2F(m_outColorAspects, range).inRange(
                            C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
                C2F(m_outColorAspects, primaries).inRange(
                            C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
                C2F(m_outColorAspects, transfer).inRange(
                            C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
                C2F(m_outColorAspects, matrix).inRange(
                            C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
            })
            .withSetter(ColorAspectsSetter, m_defaultColorAspects, m_inColorAspects)
            .build());
    }
    // In order to pass CTS, we advertise support for these formats defined by Google. Because they
    // don't recognize HAL implementation specific formats.
    // But the format we actually allocated buffer is HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL for 8-bit,
    // HAL_PIXEL_FORMAT_P010_INTEL for 10-bit. via function MfxFourCCToGralloc in mfx_c2_utils.cpp
    std::vector<uint32_t> supportedPixelFormats = {
        HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED,
        HAL_PIXEL_FORMAT_YCBCR_420_888,
        HAL_PIXEL_FORMAT_YV12,
        HAL_PIXEL_FORMAT_YCBCR_P010
    };

    addParameter(
        DefineParam(m_pixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
        .withDefault(new C2StreamPixelFormatInfo::output(
                            0u, HAL_PIXEL_FORMAT_YCBCR_420_888))
        .withFields({C2F(m_pixelFormat, value).oneOf(supportedPixelFormats)})
        .withSetter((Setter<decltype(*m_pixelFormat)>::StrictValueWithNoDeps))
        .build());

    // HDR static with BT2020 by default
    m_hdrStaticInfo = std::make_shared<C2StreamHdrStaticInfo::output>();
    m_hdrStaticInfo->mastering = {
        .red   = { .x = 0.708,  .y = 0.292 },
        .green = { .x = 0.170,  .y = 0.797 },
        .blue  = { .x = 0.131,  .y = 0.046 },
        .white = { .x = 0.3127, .y = 0.3290 },
        .maxLuminance = 0,
        .minLuminance = 0,
        };
    m_hdrStaticInfo->maxCll = 0;
    m_hdrStaticInfo->maxFall = 0;

    MFX_ZERO_MEMORY(m_signalInfo);
    MFX_ZERO_MEMORY(m_secureCodec);
    //m_paramStorage.DumpParams();
}

MfxC2DecoderComponent::~MfxC2DecoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    Release();
}

void MfxC2DecoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("c2.intel.avc.decoder",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_H264>);

    registry.RegisterMfxC2Component("c2.intel.hevc.decoder",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_H265>);

    registry.RegisterMfxC2Component("c2.intel.vp9.decoder",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_VP9>);

    registry.RegisterMfxC2Component("c2.intel.vp8.decoder",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_VP8>);

    registry.RegisterMfxC2Component("c2.intel.mp2.decoder",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_MPEG2>);

    registry.RegisterMfxC2Component("c2.intel.av1.decoder",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_AV1>);
}

c2_status_t MfxC2DecoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MfxDev::Create(MfxDev::Usage::Decoder, &m_device);

    if(mfx_res == MFX_ERR_NONE) {
        mfx_res = ResetSettings(); // requires device_ initialized
    }

    if(mfx_res == MFX_ERR_NONE) {
        mfx_res = InitSession();
    }

    if(MFX_ERR_NONE == mfx_res) {
        InitFrameConstructor();
    }

    return MfxStatusToC2(mfx_res);
}

// Dump input/output for decoder/encoder steps:
//
// 1. $adb shell
// 2. In adb shell, $setenforce 0
// 3. In adb shell,
//
//    Case 1) For decoder input dump: $setprop vendor.c2.dump.decoder.input true
//    Case 2) For decoder output dump: $setprop vendor.c2.dump.decoder.output.number 10
//    Case 3) For encoder input dump: $setprop vendor.c2.dump.encoder.input.number 10
//    Case 4) For encoder output dump: $setprop vendor.c2.dump.encoder.output true
//
//    yuv frames number to dump can be set as needed.
//
// 4. Run decode/encoder, when done, dumped files can be found in /data/local/traces.
//
//    For cases 1), 3) and 4), for one bitsteam decode or one yuv file encode, can
//    only dump once, dumped files are named in:
//    "decoder/encoder name - year - month - day - hour -minute - second".
//    Dumped files will not be automatically deleted/overwritten in next run, will
//    need be deleted manually.
//
//    For case 2), for one bitstream decode, output can be dumped multiple times
//    during decode process, setprop need be run again for each dump, dumped files
//    are named in "xxx_0.yuv", "xxx_1.yuv" ...
//
// 5. When viewing yuv frames, check surface width & height in logs.

static void InitDump(std::unique_ptr<BinaryWriter>& input_writer,
                     std::shared_ptr<C2ComponentNameSetting> decoder_name)
{
    MFX_DEBUG_TRACE_FUNC;

    bool dump_input_enabled = false;
    char* value = new char[20];
    if(property_get(DECODER_DUMP_INPUT_KEY, value, NULL) > 0) {
        if(strcasecmp(value, "true") == 0) {
            dump_input_enabled = true;
            property_set(DECODER_DUMP_INPUT_KEY, NULL);
        }
    }
    delete[] value;

    if (dump_input_enabled) {
        std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::ostringstream file_name;
        std::tm local_tm;
        localtime_r(&now_c, &local_tm);

        file_name << decoder_name->m.value << "-" <<
                std::put_time(std::localtime(&now_c), "%Y-%m-%d-%H-%M-%S") << ".bin";

        MFX_DEBUG_TRACE_STREAM("Decoder input dump is started to " <<
                MFX_C2_DUMP_DIR << "/" << MFX_C2_DUMP_DECODER_SUB_DIR << "/"
		<<MFX_C2_DUMP_INPUT_SUB_DIR << "/" << file_name.str());

        input_writer = std::make_unique<BinaryWriter>(MFX_C2_DUMP_DIR,
                std::vector<std::string>({MFX_C2_DUMP_DECODER_SUB_DIR,
                MFX_C2_DUMP_INPUT_SUB_DIR}), file_name.str());
    }
}

c2_status_t MfxC2DecoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    m_uSyncedPointsCount = 0;
    m_bEosReceived = false;
    m_bAllocatorSet = false;

    do {
        // Must reserve MFXClose(), InitSession() logic here, if not some drm cts will not pass
#ifdef USE_ONEVPL
        mfxStatus mfx_res = MFXClose(m_mfxSession);
#else
        mfxStatus mfx_res = m_mfxSession.Close();
#endif
        if (MFX_ERR_NONE != mfx_res) break;

        mfx_res = InitSession();
        if (MFX_ERR_NONE != mfx_res) break;

        InitDump(m_inputWriter, m_name);

        m_workingQueue.Start();
        m_waitingQueue.Start();
    } while(false);

    m_OperationState = OperationState::RUNNING;

    return C2_OK;
}

c2_status_t MfxC2DecoderComponent::DoStop(bool abort)
{
    MFX_DEBUG_TRACE_FUNC;
    m_OperationState = OperationState::STOPPING;
    // Working queue should stop first otherwise race condition
    // is possible when waiting queue is stopped (first), but working
    // queue is pushing tasks into it (via DecodeFrameAsync). As a
    // result, such tasks will be processed after next start
    // which is bad as sync point becomes invalid after
    // decoder Close/Init.
    if (abort) {
        m_workingQueue.Abort();
        m_waitingQueue.Abort();
    } else {
        m_workingQueue.Stop();
        m_waitingQueue.Stop();
    }

    {
        std::lock_guard<std::mutex> lock(m_pendingWorksMutex);
        for (auto& pair : m_pendingWorks) {
            // Other statuses cause libstagefright_ccodec fatal error
            NotifyWorkDone(std::move(pair.second), C2_NOT_FOUND);
        }
        m_pendingWorks.clear();
    }

    m_c2Allocator = nullptr;

    m_readViews.clear();
    m_duplicatedTimeStamp.clear();

    FreeDecoder();

    if (m_c2Bitstream) {
        m_c2Bitstream->Reset();
        m_c2Bitstream->GetFrameConstructor()->Close();
    }

    if(m_outputWriter.get() != NULL && m_outputWriter->IsOpen()) {
        m_outputWriter->Close();
    }
    m_outputWriter.reset();

    if(m_inputWriter.get() != NULL && m_inputWriter->IsOpen()) {
        m_inputWriter->Close();
    }
    m_inputWriter.reset();

    m_OperationState = OperationState::STOPPED;
    return C2_OK;
}

c2_status_t MfxC2DecoderComponent::Release()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    mfxStatus sts = MFX_ERR_NONE;

#ifdef USE_ONEVPL
    if (m_mfxSession) {
        sts = MFXClose(m_mfxSession);
        m_mfxSession = nullptr;
    }
#else
    sts = m_mfxSession.Close();
    if (MFX_ERR_NONE != sts) res = MfxStatusToC2(sts);
#endif


    if (m_allocator) {
        m_allocator = nullptr;
    }

    if (m_device) {
        m_device->Close();

        m_device = nullptr;
    }

#ifdef USE_ONEVPL
    if (m_mfxLoader) {
        MFXUnload(m_mfxLoader);
        m_mfxLoader = nullptr;
    }
#endif

    return res;
}

void MfxC2DecoderComponent::InitFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2FrameConstructorType fc_type;
    switch (m_decoderType)
    {
    case DECODER_H264_SECURE:
    case DECODER_H264:
        fc_type = MfxC2FC_AVC;
        break;
    case DECODER_H265_SECURE:
    case DECODER_H265:
        fc_type = MfxC2FC_HEVC;
        break;
    case DECODER_VP9:
        fc_type = MfxC2FC_VP9;
        break;
    case DECODER_VP8:
        fc_type = MfxC2FC_VP8;
        break;
    case DECODER_MPEG2:
        fc_type = MfxC2FC_MP2;
        break;
    case DECODER_AV1:
        fc_type = MfxC2FC_AV1;
        break;
    default:
        MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
        fc_type = MfxC2FC_None;
        break;
    }
    m_c2Bitstream = std::make_unique<MfxC2BitstreamIn>(fc_type);
}

#ifdef USE_ONEVPL
mfxStatus MfxC2DecoderComponent::InitSession()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;
    mfxConfig cfg[2];
    mfxVariant cfgVal[2];

    if (nullptr == m_mfxLoader)
        m_mfxLoader = MFXLoad();

    if (nullptr == m_mfxLoader) {
        ALOGE("MFXLoad failed...is implementation in path?");
        return MFX_ERR_UNKNOWN;
    }

    /* Create configurations for implementation */
    cfg[0] = MFXCreateConfig(m_mfxLoader);
    if (!cfg[0]) {
        ALOGE("Failed to create a MFX configuration");
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    cfgVal[0].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[0].Data.U32 = (m_mfxImplementation == MFX_IMPL_SOFTWARE) ? MFX_IMPL_TYPE_SOFTWARE : MFX_IMPL_TYPE_HARDWARE;
    mfx_res = MFXSetConfigFilterProperty(cfg[0], (const mfxU8 *) "mfxImplDescription.Impl", cfgVal[0]);
    if (MFX_ERR_NONE != mfx_res) {
        ALOGE("Failed to add an additional MFX configuration (%d)", mfx_res);
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    cfg[1] = MFXCreateConfig(m_mfxLoader);
    if (!cfg[1]) {
        ALOGE("Failed to create a MFX configuration");
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    cfgVal[1].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[1].Data.U32 = MFX_VERSION;
    mfx_res = MFXSetConfigFilterProperty(cfg[1], (const mfxU8 *) "mfxImplDescription.ApiVersion.Version", cfgVal[1]);
    if (MFX_ERR_NONE != mfx_res) {
        ALOGE("Failed to add an additional MFX configuration (%d)", mfx_res);
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    while (1) {
        /* Enumerate all implementations */
        uint32_t idx = 0;
        mfxImplDescription *idesc;
        mfx_res = MFXEnumImplementations(m_mfxLoader, idx, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL *)&idesc);

        if (MFX_ERR_NOT_FOUND == mfx_res) {
            /* Failed to find an available implementation */
            break;
        }
        else if (MFX_ERR_NONE != mfx_res) {
            /*implementation found, but requested query format is not supported*/
            idx++;
            continue;
        }
        MFX_LOG_INFO("%s. Idx = %d. ApiVersion: %d.%d. Implementation type: %s. AccelerationMode via: %d",
                 __func__, idx, idesc->ApiVersion.Major, idesc->ApiVersion.Minor,
                (idesc->Impl == MFX_IMPL_TYPE_SOFTWARE) ? "SW" : "HW",
                idesc->AccelerationMode);

        mfx_res = MFXCreateSession(m_mfxLoader, idx, &m_mfxSession);

        MFXDispReleaseImplDescription(m_mfxLoader, idesc);

        if (MFX_ERR_NONE == mfx_res)
            break;

        idx++;
    }

    if (MFX_ERR_NONE != mfx_res)
    {
        if (!m_mfxLoader)
            MFXUnload(m_mfxLoader);

        MFX_LOG_ERROR("Failed to create a MFX session (%d)", mfx_res);
        return mfx_res;
    }

    mfx_res = m_device->InitMfxSession(m_mfxSession);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}
#else
mfxStatus MfxC2DecoderComponent::InitSession()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    do {
        mfx_res = m_mfxSession.Init(m_mfxImplementation, &g_required_mfx_version);
        if (MFX_ERR_NONE != mfx_res) {
            MFX_DEBUG_TRACE_MSG("MFXVideoSession::Init failed");
            break;
        }
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Major);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Minor);

        mfx_res = m_mfxSession.QueryIMPL(&m_mfxImplementation);
        if (MFX_ERR_NONE != mfx_res) break;
        MFX_DEBUG_TRACE_I32(m_mfxImplementation);

        mfx_res = m_device->InitMfxSession(&m_mfxSession);

    } while (false);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}
#endif

mfxStatus MfxC2DecoderComponent::ResetSettings()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;
    MFX_ZERO_MEMORY(m_mfxVideoParams);
    MFX_ZERO_MEMORY(m_signalInfo);
    MFX_ZERO_MEMORY(m_secureCodec);

    m_signalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
    m_signalInfo.Header.BufferSz = sizeof(mfxExtVideoSignalInfo);
    m_secureCodec.Header.BufferId = MFX_EXTBUFF_SECURE_CODEC;
    m_secureCodec.Header.BufferSz = sizeof(mfxExtSecureCodec);
    m_secureCodec.on = m_secure;

    switch (m_decoderType)
    {
    case DECODER_H264_SECURE:
    case DECODER_H264:
        m_mfxVideoParams.mfx.CodecId = MFX_CODEC_AVC;
        break;
    case DECODER_H265_SECURE:
    case DECODER_H265:
        m_mfxVideoParams.mfx.CodecId = MFX_CODEC_HEVC;
        break;
    case DECODER_VP9:
        m_mfxVideoParams.mfx.CodecId = MFX_CODEC_VP9;
        break;
    case DECODER_VP8:
        m_mfxVideoParams.mfx.CodecId = MFX_CODEC_VP8;
        break;
    case DECODER_MPEG2:
        m_mfxVideoParams.mfx.CodecId = MFX_CODEC_MPEG2;
        break;
    case DECODER_AV1:
        m_mfxVideoParams.mfx.CodecId = MFX_CODEC_AV1;
        break;
    default:
        MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
        break;
    }

    m_signalInfo.VideoFullRange = 2; // UNSPECIFIED Range
    mfx_set_defaults_mfxVideoParam_dec(&m_mfxVideoParams);

    m_mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

    return res;
}

mfxU16 MfxC2DecoderComponent::GetAsyncDepth(void)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxU16 asyncDepth;

    // AV1 decoder requires more reference frame buffers.
#ifdef USE_ONEVPL
    if (MFX_CODEC_AV1 == m_mfxVideoParams.mfx.CodecId)
        asyncDepth = (MFX_IMPL_BASETYPE(m_mfxImplementation) == MFX_IMPL_SOFTWARE) ? 0 : 10;
    else
        asyncDepth = (MFX_IMPL_BASETYPE(m_mfxImplementation) == MFX_IMPL_SOFTWARE) ? 0 : 1;
#else
    if ((MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(m_mfxImplementation)) &&
        ((MFX_CODEC_AVC == m_mfxVideoParams.mfx.CodecId) ||
         (MFX_CODEC_HEVC == m_mfxVideoParams.mfx.CodecId) ||
         (MFX_CODEC_VP9 == m_mfxVideoParams.mfx.CodecId) ||
         (MFX_CODEC_VP8 == m_mfxVideoParams.mfx.CodecId) ||
         (MFX_CODEC_MPEG2 == m_mfxVideoParams.mfx.CodecId)))
        asyncDepth = 1;
    else if ((MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(m_mfxImplementation)) &&
         (MFX_CODEC_AV1 == m_mfxVideoParams.mfx.CodecId))
        asyncDepth = 10;
    else
        asyncDepth = 0;
#endif

    MFX_DEBUG_TRACE_I32(asyncDepth);
    return asyncDepth;
}

mfxStatus MfxC2DecoderComponent::InitDecoder(std::shared_ptr<C2BlockPool> c2_allocator)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;
    // Workaround for MSDK issue which would change crop size on AV1.
    mfxU16 cropW = 0, cropH = 0;
    std::lock_guard<std::mutex> lock(m_initDecoderMutex);

    {
        MFX_DEBUG_TRACE_MSG("InitDecoder: DecodeHeader");

        if (nullptr == m_mfxDecoder) {
#ifdef USE_ONEVPL
            m_mfxDecoder.reset(MFX_NEW_NO_THROW(MFXVideoDECODE(m_mfxSession)));
#else
            m_mfxDecoder.reset(MFX_NEW_NO_THROW(MFXVideoDECODE(m_mfxSession)));
#endif
            if (nullptr == m_mfxDecoder) {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        }

        if (m_secure) {
            m_consumerUsage |= kProtectedUsage;
        }

        if (MFX_ERR_NONE == mfx_res) {
            // saving parameters
            mfxVideoParam oldParams = m_mfxVideoParams;

            m_extBuffers.push_back(reinterpret_cast<mfxExtBuffer*>(&m_signalInfo));
            m_mfxVideoParams.NumExtParam = m_extBuffers.size();
            m_mfxVideoParams.ExtParam = &m_extBuffers.front();

            // decoding header
            mfx_res = m_mfxDecoder->DecodeHeader(m_c2Bitstream->GetFrameConstructor()->GetMfxBitstream().get(), &m_mfxVideoParams);
            // MSDK will call the function av1_native_profile_to_mfx_profile to change CodecProfile in DecodeHeader while
            // decoder type is av1. So after calling DecodeHeader, we have to revert this value to avoid unexpected behavior.
            if (m_decoderType == DECODER_AV1)
                m_mfxVideoParams.mfx.CodecProfile = av1_mfx_profile_to_native_profile(m_mfxVideoParams.mfx.CodecProfile);

            m_extBuffers.pop_back();
            m_mfxVideoParams.NumExtParam = oldParams.NumExtParam;
            m_mfxVideoParams.ExtParam = oldParams.ExtParam;
        }
        if (MFX_ERR_NULL_PTR == mfx_res) {
            mfx_res = MFX_ERR_MORE_DATA;
        }

        if (MFX_ERR_NONE == mfx_res) {
            MFX_DEBUG_TRACE_I32(m_mfxVideoParams.IOPattern);
            MFX_DEBUG_TRACE_I32(m_mfxVideoParams.mfx.FrameInfo.Width);
            MFX_DEBUG_TRACE_I32(m_mfxVideoParams.mfx.FrameInfo.Height);

            // Query required surfaces number for decoder
            mfxFrameAllocRequest decRequest = {};
            mfx_res = m_mfxDecoder->QueryIOSurf(&m_mfxVideoParams, &decRequest);
            if (MFX_ERR_NONE == mfx_res) {
                if (m_uOutputDelay < decRequest.NumFrameSuggested) {
                    MFX_DEBUG_TRACE_MSG("More buffer needed for decoder output!");
                    ALOGE("More buffer needed for decoder output! Actual: %d. Expected: %d",
                        m_uOutputDelay, decRequest.NumFrameSuggested);
                    mfx_res = MFX_ERR_MORE_SURFACE;
                }
                m_surfaceNum = MFX_MAX(decRequest.NumFrameSuggested, 4);
                MFX_DEBUG_TRACE_U32(decRequest.NumFrameSuggested);
                MFX_DEBUG_TRACE_U32(decRequest.NumFrameMin);
                MFX_DEBUG_TRACE_U32(m_surfaceNum);
            } else {
                MFX_DEBUG_TRACE_MSG("QueryIOSurf failed");
                mfx_res = MFX_ERR_UNKNOWN;
            }
        }

        m_mfxVideoParams.mfx.FrameInfo.Width = MFX_MEM_ALIGN(m_mfxVideoParams.mfx.FrameInfo.Width, 16);
        m_mfxVideoParams.mfx.FrameInfo.Height = MFX_MEM_ALIGN(m_mfxVideoParams.mfx.FrameInfo.Height, 16);
        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = m_c2Bitstream->GetFrameConstructor()->Init(m_mfxVideoParams.mfx.CodecProfile, m_mfxVideoParams.mfx.FrameInfo);
        }
    }

    if (MFX_ERR_NONE == mfx_res) {
        MFX_DEBUG_TRACE_MSG("InitDecoder: UpdateColorAspectsFromBitstream");
        UpdateColorAspectsFromBitstream(m_signalInfo);

        MFX_DEBUG_TRACE_MSG("InitDecoder: GetAsyncDepth");
        m_mfxVideoParams.AsyncDepth = GetAsyncDepth();
    }

    // We need check whether the BQ allocator has a surface, if No we cannot use MFX_IOPATTERN_OUT_VIDEO_MEMORY mode.
    if (MFX_ERR_NONE == mfx_res && m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
        std::shared_ptr<C2GraphicBlock> out_block;
        c2_status_t res = C2_OK;
        C2MemoryUsage mem_usage = {C2AndroidMemoryUsage::CPU_READ | C2AndroidMemoryUsage::HW_COMPOSER_READ,
                                C2AndroidMemoryUsage::HW_CODEC_WRITE};

        res = m_c2Allocator->fetchGraphicBlock(m_mfxVideoParams.mfx.FrameInfo.Width,
                                            m_mfxVideoParams.mfx.FrameInfo.Height,
                                            MfxFourCCToGralloc(m_mfxVideoParams.mfx.FrameInfo.FourCC),
                                            mem_usage, &out_block);

        if (res == C2_OK) {
// Before android 15, it's using BUFFER QUEUE for surface allocation, check if
// surface is allocated by BUFFER QUEUE with igbp_id and igbp_slot
#if PLATFORM_SDK_VERSION <= 34
            uint32_t width, height, format, stride, igbp_slot, generation;
            uint64_t usage, igbp_id;
            android::_UnwrapNativeCodec2GrallocMetadata(out_block->handle(), &width, &height, &format, &usage,
                                                        &stride, &generation, &igbp_id, &igbp_slot);
            if ((!igbp_id && !igbp_slot) || (!igbp_id && igbp_slot == 0xffffffff)) {
                // No surface & BQ
                m_mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            }
#else // For android 15, it's using IGBA and Ahwb buffer for surface allocation
            if(!C2AllocatorAhwb::CheckHandle(out_block->handle())) {
                m_mfxVideoParams.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
            }
#endif
        }
    }

    bool allocator_required = (m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY);
    if (MFX_ERR_NONE == mfx_res && allocator_required && m_bAllocatorSet == false) {
        // set frame allocator
        m_allocator = m_device->GetFramePoolAllocator();
#ifdef USE_ONEVPL
        mfx_res = MFXVideoCORE_SetFrameAllocator(m_mfxSession, &(m_device->GetFrameAllocator()->GetMfxAllocator()));
#else
        mfx_res = m_mfxSession.SetFrameAllocator(&(m_device->GetFrameAllocator()->GetMfxAllocator()));
#endif
        if (MFX_ERR_NONE == mfx_res) m_bAllocatorSet = true;
    }

    if (MFX_ERR_NONE == mfx_res) {
        MFX_DEBUG_TRACE_MSG("InitDecoder: Init");

        MFX_DEBUG_TRACE__mfxVideoParam_dec(m_mfxVideoParams);
        cropW = m_mfxVideoParams.mfx.FrameInfo.CropW;
        cropH = m_mfxVideoParams.mfx.FrameInfo.CropH;

        if (m_allocator) {
            m_allocator->SetC2Allocator(std::move(c2_allocator));
            m_allocator->SetBufferCount(m_uOutputDelay);
            m_allocator->SetConsumerUsage(m_consumerUsage);
        }

        mfxVideoParam oldParams = m_mfxVideoParams;
        m_extBuffers.push_back(reinterpret_cast<mfxExtBuffer*>(&m_secureCodec));
        m_mfxVideoParams.NumExtParam = m_extBuffers.size();
        m_mfxVideoParams.ExtParam = &m_extBuffers.front();
        MFX_DEBUG_TRACE_MSG("Decoder initializing...");
        mfx_res = m_mfxDecoder->Init(&m_mfxVideoParams);
        MFX_DEBUG_TRACE_PRINTF("Decoder initialized, sts = %d", mfx_res);
        m_extBuffers.pop_back();
        m_mfxVideoParams.NumExtParam = oldParams.NumExtParam;
        m_mfxVideoParams.ExtParam = oldParams.ExtParam;

        // c2 allocator is needed to handle mfxAllocRequest coming from m_mfxDecoder->Init,
        // not needed after that.
        if (m_allocator) m_allocator->SetC2Allocator(nullptr);

        if (MFX_WRN_PARTIAL_ACCELERATION == mfx_res) {
            MFX_DEBUG_TRACE_MSG("InitDecoder returns MFX_WRN_PARTIAL_ACCELERATION");
            mfx_res = MFX_ERR_NONE;
        } else if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfx_res) {
            MFX_DEBUG_TRACE_MSG("InitDecoder returns MFX_WRN_INCOMPATIBLE_VIDEO_PARAM");
            mfx_res = MFX_ERR_NONE;
        }
        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = m_mfxDecoder->GetVideoParam(&m_mfxVideoParams);

            m_uMaxWidth = m_mfxVideoParams.mfx.FrameInfo.Width;
            m_uMaxHeight = m_mfxVideoParams.mfx.FrameInfo.Height;

            MFX_DEBUG_TRACE__mfxVideoParam_dec(m_mfxVideoParams);
        }

        if (MFX_ERR_NONE == mfx_res) {
            m_bInitialized = true;
            m_bInitialized_once = true;
        }
        // Same as DecodeHeader, MSDK will call the function av1_native_profile_to_mfx_profile to change CodecProfile
        // in Init().
        if (m_decoderType == DECODER_AV1) {
            m_mfxVideoParams.mfx.CodecProfile = av1_mfx_profile_to_native_profile(m_mfxVideoParams.mfx.CodecProfile);
            m_mfxVideoParams.mfx.FrameInfo.CropW = cropW;
            m_mfxVideoParams.mfx.FrameInfo.CropH = cropH;
        }
    }

    // Google requires the component to decode to 8-bit color format by default.
    // Reference CTS cases testDefaultOutputColorFormat.
    if (MFX_ERR_NONE == mfx_res && HAL_PIXEL_FORMAT_YCBCR_420_888 == m_pixelFormat->value &&
            MFX_FOURCC_P010 == m_mfxVideoParams.mfx.FrameInfo.FourCC) {
        m_vppConversion = true;
        mfx_res = InitVPP();
    }

    if (MFX_ERR_NONE != mfx_res) {
        FreeDecoder();
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2DecoderComponent::InitVPP()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;
    mfxVideoParam vppParam;

#ifdef USE_ONEVPL
        MFX_NEW(m_vpp, MFXVideoVPP(m_mfxSession));
#else
        MFX_NEW(m_vpp, MFXVideoVPP(*m_pSession));
#endif

    if(!m_vpp) {
        sts = MFX_ERR_UNKNOWN;
        return sts;
    }

    mfxFrameInfo frame_info;
    frame_info = m_mfxVideoParams.mfx.FrameInfo;
    MFX_ZERO_MEMORY(vppParam);

    if(m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY) {
        vppParam.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY | MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
    } else {
        vppParam.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY | MFX_IOPATTERN_OUT_VIDEO_MEMORY;
    }

    vppParam.vpp.In = frame_info;
    vppParam.vpp.Out = frame_info;

    vppParam.vpp.Out.FourCC = MFX_FOURCC_NV12;
    vppParam.vpp.Out.Shift = 0;

    vppParam.AsyncDepth = m_mfxVideoParams.AsyncDepth;

    MFX_DEBUG_TRACE__mfxFrameInfo(vppParam.vpp.In);
    MFX_DEBUG_TRACE__mfxFrameInfo(vppParam.vpp.Out);

    if (MFX_ERR_NONE == sts) sts = m_vpp->Init(&vppParam);

    return sts;
}

void MfxC2DecoderComponent::FreeDecoder()
{
    MFX_DEBUG_TRACE_FUNC;

    m_bInitialized = false;

    m_lockedSurfaces.clear();

    if(m_vppConversion && m_vpp != NULL) {
        m_vpp->Close();
        MFX_DELETE(m_vpp);
    }

    if(nullptr != m_mfxDecoder) {
        m_mfxDecoder->Close();
        m_mfxDecoder = nullptr;
    }

    m_uMaxHeight = 0;
    m_uMaxWidth = 0;
    m_surfaceNum = 0;

    FreeSurfaces();

    if (m_allocator) {
        m_allocator->Reset();
    }
}

void MfxC2DecoderComponent::FreeSurfaces()
{
    MFX_DEBUG_TRACE_FUNC;

    m_surfaces.clear();
    m_blocks.clear();
    m_surfacePool.clear();
}

mfxStatus MfxC2DecoderComponent::HandleFormatChange(bool need_reset)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    if (!m_mfxDecoder || !m_c2Bitstream) return MFX_ERR_NULL_PTR;

    mfx_res = m_mfxDecoder->DecodeHeader(m_c2Bitstream->GetFrameConstructor()->GetMfxBitstream().get(), &m_mfxVideoParams);
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    // when oneVPL returns an error of resolution change, it is not necessarily the width or height has changed,
    // it may be the profile or other SPS paramters.

    // Free all the surfaces
    mfx_res = m_mfxDecoder->Close();
    if (MFX_ERR_NONE == mfx_res) {
        // De-allocate all the surfaces
        m_lockedSurfaces.clear();
        FreeSurfaces();
        if (m_allocator) {
            m_allocator->Reset();
        }
        // Re-init decoder
        m_bInitialized = false;
        m_uMaxWidth = m_mfxVideoParams.mfx.FrameInfo.Width;
        m_uMaxHeight = m_mfxVideoParams.mfx.FrameInfo.Height;
    }

    if ((DECODER_H264 == m_decoderType || DECODER_H265 == m_decoderType) && need_reset) {
        MFX_DEBUG_TRACE_MSG("Reset BitStream for re-append header.");
        m_c2Bitstream->Reset();
    }

    return mfx_res;
}

c2_status_t MfxC2DecoderComponent::UpdateC2Param(const mfxVideoParam* src, C2Param::Index index) const
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    if (C2PortSurfaceAllocatorTuning::output::PARAM_TYPE == index) {
        m_surfaceAllocator->value = C2PlatformAllocatorStore::BUFFERQUEUE;
        MFX_DEBUG_TRACE_PRINTF("Set output port surface alloctor to: %d", m_surfaceAllocator->value);
    }

    switch (index.typeIndex()) {
        case kParamIndexBlockPools: {
            m_outputPoolIds->m.values[0] = m_outputPoolId;
            break;
        }
        case kParamIndexPictureSize: {
            if (C2StreamPictureSizeInfo::output::PARAM_TYPE == index) {
                MFX_DEBUG_TRACE("GetPictureSize");
                m_size->width = src->mfx.FrameInfo.Width;
                m_size->height = src->mfx.FrameInfo.Height;
                MFX_DEBUG_TRACE_STREAM(NAMED(m_size->width) << NAMED(m_size->height));
            }
            break;
        }
        case kParamIndexAllocators: {
            if (C2PortAllocatorsTuning::output::PARAM_TYPE == index) {
                m_outputAllocators->m.values[0] = C2PlatformAllocatorStore::GRALLOC;
                MFX_DEBUG_TRACE_PRINTF("Set output port alloctor to: %d", m_outputAllocators->m.values[0]);
            }
            break;
        }
        default:
            MFX_DEBUG_TRACE_STREAM("attempt to query "
                            << index.typeIndex() << " type, but not found.");
            break;
    }

    return res;
}

c2_status_t MfxC2DecoderComponent::UpdateMfxParamToC2(
    std::unique_lock<std::mutex> state_lock,
    const std::vector<C2Param*> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    (void)state_lock;
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(m_initDecoderMutex);

    c2_status_t res = C2_OK;
    c2_status_t param_res = C2_OK;

    // determine source, update it if needed
    const mfxVideoParam* params_view = &m_mfxVideoParams;
    if (nullptr != params_view) {
        // 1st cycle on stack params
        for (C2Param* param : stackParams) {
            param_res = UpdateC2Param(params_view, param->index());
            if (param_res != C2_OK) {
                param->invalidate();
                res = param_res;
            }
        }
        param_res = C2_OK;
        // 2nd cycle on heap params
        for (C2Param::Index param_index : heapParamIndices) {
            // allocate in UpdateC2Param
            C2Param* param = nullptr;
            param_res = UpdateC2Param(params_view, param_index);
            if (param_res == C2_OK) {
                if(nullptr != heapParams) {
                    heapParams->push_back(std::unique_ptr<C2Param>(param));
                } else {
                    MFX_DEBUG_TRACE_MSG("heapParams is null");
                    res = C2_BAD_VALUE;
                }
            } else {
                delete param;
                res = param_res;
            }
        }
    } else {
        // no params_view was acquired
        for (C2Param* param : stackParams) {
            param->invalidate();
        }
        res = C2_CORRUPTED;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

void MfxC2DecoderComponent::DoUpdateMfxParam(const std::vector<C2Param*> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures,
    bool )
{
    MFX_DEBUG_TRACE_FUNC;

    for (const C2Param* param : params) {
        // applying parameter
        switch (C2Param::Type(param->type()).typeIndex()) {
            case kParamIndexPictureSize: {
                if (C2StreamPictureSizeInfo::output::PARAM_TYPE == param->index()) {
                    MFX_DEBUG_TRACE("SetPictureSize");
                    MFX_DEBUG_TRACE_STREAM(NAMED(m_size->width) << NAMED(m_size->height));
                    m_mfxVideoParams.mfx.FrameInfo.Width = m_size->width;
                    m_mfxVideoParams.mfx.FrameInfo.Height = m_size->height;
                    m_mfxVideoParams.mfx.FrameInfo.CropW = m_size->width;
                    m_mfxVideoParams.mfx.FrameInfo.CropH = m_size->height;
                }
                break;
            }
            case kParamIndexBlockPools: {
                if (m_outputPoolIds && m_outputPoolIds->flexCount() >= 1) {
                    m_outputPoolId = m_outputPoolIds->m.values[0];
                    MFX_DEBUG_TRACE_STREAM("config kParamIndexBlockPools to " << m_outputPoolId);
                } else {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexUsage: {
                if (C2StreamUsageTuning::output::PARAM_TYPE == param->index()) {
                    m_consumerUsage = m_outputUsage->value;
                    /* Do not set IO pattern during decoding, only set IO pattern in
                       the first time decoder initialization */
                    if (!m_bInitialized_once) {
                        // Set memory type according to consumer usage sent from framework
                        m_mfxVideoParams.IOPattern = (m_consumerUsage & (C2MemoryUsage::CPU_READ | C2MemoryUsage::CPU_WRITE)) ?
                            MFX_IOPATTERN_OUT_SYSTEM_MEMORY : MFX_IOPATTERN_OUT_VIDEO_MEMORY;
                    }
                    MFX_DEBUG_TRACE_STREAM("config kParamIndexUsage to 0x" << std::hex << m_consumerUsage);
                }
                break;
            }
            default:
                MFX_DEBUG_TRACE_STREAM("attempt to configure "
                                    << C2Param::Type(param->type()).typeIndex() << " type, but not found");
                break;
        }
    }
}

c2_status_t MfxC2DecoderComponent::UpdateC2ParamToMfx(std::unique_lock<std::mutex> state_lock,
    const std::vector<C2Param*> &params,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures) {

    (void)state_lock;
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if (nullptr == failures) {
            res = C2_CORRUPTED; break;
        }

        failures->clear();

        std::lock_guard<std::mutex> lock(m_initDecoderMutex);

        DoUpdateMfxParam(params, failures, true);

        res = GetAggregateStatus(failures);

    } while(false);

    return res;
}

mfxStatus MfxC2DecoderComponent::DecodeFrameAsync(
    mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,
    mfxSyncPoint *syncp)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    int trying_count = 0;
    const int MAX_TRYING_COUNT = 200;
    const auto timeout = std::chrono::milliseconds(5);

    {
        // Prevent msdk decoder from overfeeding.
        // It may result in all allocated surfaces become locked, nothing to supply as surface_work
        // to DecodeFrameAsync.
        std::unique_lock<std::mutex> lock(m_devBusyMutex);
        m_devBusyCond.wait(lock, [this] { return m_uSyncedPointsCount < m_mfxVideoParams.AsyncDepth; } );
        // Can release the lock here as m_uSyncedPointsCount is incremented in this thread,
        // so condition cannot go to false.
    }

    do {
        mfx_res = m_mfxDecoder->DecodeFrameAsync(bs, surface_work, surface_out, syncp);
        ++trying_count;

        if (MFX_WRN_DEVICE_BUSY == mfx_res) {
            // If surface is locked, the last decoding task may have been submitted on oneVPL/MSDK.
            // Calling again DecodeFrameAsync with this locked surface will cause the bs to be read
            // and offset will be wrong.
            // This issue will not happen on MSDK. Because MSDK will jugde whether surface is locked first,
            // while oneVPL is oppsite.
            if (surface_work->Data.Locked) {
                mfx_res = MFX_ERR_MORE_SURFACE;
                break;
            }

            if (m_bFlushing) {
                // break waiting as flushing in progress and return MFX_WRN_DEVICE_BUSY as sign of it
                break;
            }

            if (trying_count >= MAX_TRYING_COUNT) {
                MFX_DEBUG_TRACE_MSG("Too many MFX_WRN_DEVICE_BUSY from DecodeFrameAsync");
                mfx_res = MFX_ERR_DEVICE_FAILED;
                break;
            }

            std::unique_lock<std::mutex> lock(m_devBusyMutex);
            unsigned int synced_points_count = m_uSyncedPointsCount;
            // wait for change of m_uSyncedPointsCount
            // that might help with MFX_WRN_DEVICE_BUSY
            m_devBusyCond.wait_for(lock, timeout, [this, synced_points_count] {
                return m_uSyncedPointsCount < synced_points_count;
            } );
            if (m_bFlushing) { // do check flushing again after timeout to not call DecodeFrameAsync once more
                break;
            }
        }
    } while (MFX_WRN_DEVICE_BUSY == mfx_res);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

// Can return MFX_ERR_NONE, MFX_ERR_MORE_DATA, MFX_ERR_MORE_SURFACE in case of success
mfxStatus MfxC2DecoderComponent::DecodeFrame(mfxBitstream *bs, MfxC2FrameOut&& frame_work,
    bool* flushing, bool* expect_output)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_sts = MFX_ERR_NONE;
    mfxFrameSurface1 *surface_work = nullptr, *surface_out = nullptr;
    *expect_output = false;
    *flushing = false;

    do {

        surface_work = frame_work.GetMfxFrameSurface().get();
        if (nullptr == surface_work) {
            mfx_sts = MFX_ERR_NULL_PTR;
            break;
        }

        MFX_DEBUG_TRACE_P(surface_work);
        if (surface_work->Data.Locked) {
            mfx_sts = MFX_ERR_UNDEFINED_BEHAVIOR;
            break;
        }

        // Set surface CropW and CropH ZERO for VP8 as oneVPL
        // doesn't do real crop if CropW and CropH is set for VP8
        if (m_mfxVideoParams.mfx.CodecId == MFX_CODEC_VP8) {
            surface_work->Info.CropW = 0;
            surface_work->Info.CropH = 0;
        }

        mfxSyncPoint sync_point;
        mfx_sts = DecodeFrameAsync(bs,
                                   surface_work,
                                   &surface_out,
                                   &sync_point);

        // valid cases for the status are:
        // MFX_ERR_NONE - data processed, output will be generated
        // MFX_ERR_MORE_DATA - data buffered, output will not be generated
        // MFX_ERR_MORE_SURFACE - need one more free surface
        // MFX_WRN_VIDEO_PARAM_CHANGED - some params changed, but decoding can be continued
        // MFX_ERR_INCOMPATIBLE_VIDEO_PARAM - need to reinitialize decoder with new params
        // status correction

        if (MFX_WRN_VIDEO_PARAM_CHANGED == mfx_sts) {
	    if (DECODER_H264 == m_decoderType || DECODER_H265 == m_decoderType) {
		mfxU16 old_range     = m_signalInfo.VideoFullRange;
		mfxU16 old_primaries = m_signalInfo.ColourPrimaries;
		mfxU16 old_transfer  = m_signalInfo.TransferCharacteristics;

		mfxFrameInfo old_frame = m_mfxVideoParams.mfx.FrameInfo;
		mfxU16 old_bitDepth    = c2_max(old_frame.BitDepthLuma, old_frame.BitDepthChroma);

		mfxExtVideoSignalInfo videoSignalInfo {};
		videoSignalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
                videoSignalInfo.Header.BufferSz = sizeof(mfxExtVideoSignalInfo);
		videoSignalInfo.VideoFullRange = 2;

                m_extBuffers.push_back(reinterpret_cast<mfxExtBuffer*>(&videoSignalInfo));
                m_mfxVideoParams.NumExtParam = m_extBuffers.size();
                m_mfxVideoParams.ExtParam = &m_extBuffers.front();

		mfxStatus mfx_res = MFX_ERR_NONE;
		mfx_res = m_mfxDecoder->GetVideoParam(&m_mfxVideoParams);

                m_extBuffers.pop_back();
		m_mfxVideoParams.NumExtParam--;

		mfxFrameInfo new_frame = m_mfxVideoParams.mfx.FrameInfo;
		mfxU16 new_bitDepth = c2_max(new_frame.BitDepthLuma, new_frame.BitDepthChroma);

		if (old_range != videoSignalInfo.VideoFullRange ||
				old_primaries != videoSignalInfo.ColourPrimaries ||
				old_transfer != videoSignalInfo.TransferCharacteristics ||
				old_bitDepth != new_bitDepth) {
		    mfx_sts = MFX_WRN_VIDEO_PARAM_CHANGED;
		} else {
		    mfx_sts = MFX_ERR_MORE_SURFACE;
		}
	    } else {
		mfx_sts = MFX_ERR_MORE_SURFACE;
	    }
	}

        if ((MFX_ERR_NONE == mfx_sts) || (MFX_ERR_MORE_DATA == mfx_sts) || (MFX_ERR_MORE_SURFACE == mfx_sts)) {

            *expect_output = true; // It might not really produce output frame from consumed bitstream,
            // but neither surface_work->Data.Locked nor mfx_sts provide exact status of that.
            MFX_DEBUG_TRACE_I32(surface_work->Data.Locked);

            // add output to waiting pool in case of Decode success only
            std::unique_lock<std::mutex> lock(m_lockedSurfacesMutex);
            m_lockedSurfaces.push_back(std::move(frame_work));

            MFX_DEBUG_TRACE_P(surface_out);
            if (nullptr != surface_out) {

                MFX_DEBUG_TRACE_I32(surface_out->Data.Locked);
                MFX_DEBUG_TRACE_I64(surface_out->Data.TimeStamp);

                if (MFX_ERR_NONE == mfx_sts) {

                    auto pred_match_surface =
                        [surface_out] (const auto& item) { return item.GetMfxFrameSurface().get() == surface_out; };

                    MfxC2FrameOut frame_out;
                    auto found = std::find_if(m_lockedSurfaces.begin(), m_lockedSurfaces.end(),
                        pred_match_surface);
                    if (found != m_lockedSurfaces.end()) {
                        frame_out = *found;
                    } else {
                        MFX_DEBUG_TRACE_STREAM("Not found LOCKED!!!");
                        mfx_sts = MFX_ERR_UNKNOWN;
                        break;
                    }
                    lock.unlock(); // unlock the mutex asap

                    m_waitingQueue.Push(
                        [ frame = std::move(frame_out), sync_point, this ] () mutable {
                        WaitWork(std::move(frame), sync_point);
                    } );
                    {
                        std::unique_lock<std::mutex> lock(m_devBusyMutex);
                        ++m_uSyncedPointsCount;
                    }
                }
            }
        } else if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mfx_sts) {
            MFX_DEBUG_TRACE_MSG("MFX_ERR_INCOMPATIBLE_VIDEO_PARAM: resolution was changed");
        }
    } while(false); // fake loop to have a cleanup point there

    if (mfx_sts == MFX_WRN_DEVICE_BUSY) {
        *flushing = true;
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
    return mfx_sts;
}

c2_status_t MfxC2DecoderComponent::AllocateC2Block(uint32_t width, uint32_t height, uint32_t fourcc, std::shared_ptr<C2GraphicBlock>* out_block, bool vpp_conversion)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {

        if (!m_c2Allocator) {
            res = C2_NOT_FOUND;
            break;
        }

        // From Android T, Surfaces are no longer used when MediaCodec#stop() is called.
        // https://android-review.googlesource.com/c/platform/frameworks/av/+/2098075
        // Exit the loop when the surface may not be available.
        if (OperationState::STOPPING == m_OperationState) {
            res = C2_CANCELED;
            break;
        }

        if (m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
            C2MemoryUsage mem_usage = {m_consumerUsage, C2AndroidMemoryUsage::HW_CODEC_WRITE};
            res = m_c2Allocator->fetchGraphicBlock(width, height,
                                               MfxFourCCToGralloc(fourcc), mem_usage, out_block);
        } else if (m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY) {
            C2MemoryUsage mem_usage = {m_consumerUsage, C2MemoryUsage::CPU_WRITE};
            res = m_c2Allocator->fetchGraphicBlock(width, height,
                                               MfxFourCCToGralloc(fourcc, false), mem_usage, out_block);
       }
    } while (res == C2_BLOCKING);

    MFX_DEBUG_TRACE_I32(res);
    return res;
}

c2_status_t MfxC2DecoderComponent::AllocateFrame(MfxC2FrameOut* frame_out, bool vpp_conversion)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::shared_ptr<MfxFrameConverter> converter;
    if (m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
        converter = m_device->GetFrameConverter();
    }

    do
    {
        auto pred_unlocked = [&](const MfxC2FrameOut &item) {
            return !item.GetMfxFrameSurface()->Data.Locked;
        };

        {
            std::lock_guard<std::mutex> lock(m_lockedSurfacesMutex);
            m_lockedSurfaces.remove_if(pred_unlocked);
        }

        std::shared_ptr<C2GraphicBlock> out_block;
        if(!vpp_conversion) {
            res = AllocateC2Block(MFXGetSurfaceWidth(m_mfxVideoParams.mfx.FrameInfo, m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY),
                                  MFXGetSurfaceHeight(m_mfxVideoParams.mfx.FrameInfo, m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY),
                                  m_mfxVideoParams.mfx.FrameInfo.FourCC, &out_block);
        } else {
            res = AllocateC2Block(MFXGetSurfaceWidth(m_mfxVideoParams.mfx.FrameInfo, m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY),
                                  MFXGetSurfaceHeight(m_mfxVideoParams.mfx.FrameInfo, m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY),
                                  MFX_FOURCC_NV12, &out_block, vpp_conversion);
        }
        if (C2_TIMED_OUT == res) continue;

        if (C2_OK != res) break;

        auto hndl_deleter = [](native_handle_t *hndl) {
            native_handle_delete(hndl);
            hndl = nullptr;
        };

        std::unique_ptr<native_handle_t, decltype(hndl_deleter)> hndl(
            android::UnwrapNativeCodec2GrallocHandle(out_block->handle()), hndl_deleter);

        if(hndl == nullptr)
        {
            return C2_NO_MEMORY;
        }
        auto it = m_surfaces.end();
        if (m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {

            uint64_t id;
            if (C2_OK != MfxGrallocInstance::getInstance()->GetBackingStore(hndl.get(), &id))
                return C2_CORRUPTED;

            it = m_surfaces.find(id);
            if (it == m_surfaces.end()){
                // haven't been used for decoding yet
                if(!vpp_conversion) {
                    res = MfxC2FrameOut::Create(converter, std::move(out_block), m_mfxVideoParams.mfx.FrameInfo, frame_out, hndl.get());
                } else {
                    mfxFrameInfo frame_info = m_mfxVideoParams.mfx.FrameInfo;
                    frame_info.FourCC = MFX_FOURCC_NV12;
                    res = MfxC2FrameOut::Create(converter, std::move(out_block), frame_info, frame_out, hndl.get());
                }
                if (C2_OK != res) {
                    break;
                }
                m_surfaces.emplace(id, frame_out->GetMfxFrameSurface());
            } else {
                if(!it->second->Data.Locked) {
                     *frame_out = MfxC2FrameOut(std::move(out_block), it->second);
                } else {
                    // when surface detached in framework, keep AllocateC2Block until we get an unlocked surface.
                    std::list<C2GraphicBlock> block_pool;
                    std::shared_ptr<C2GraphicBlock> new_block;
                    bool found_surface = false;

                    while (!found_surface) {
                        // Buffer locked, try next block.
                        res = AllocateC2Block(MFXGetSurfaceWidth(m_mfxVideoParams.mfx.FrameInfo, true),
                                      MFXGetSurfaceHeight(m_mfxVideoParams.mfx.FrameInfo, true),
                                      m_mfxVideoParams.mfx.FrameInfo.FourCC, &new_block);

                        if (C2_TIMED_OUT == res) continue;

                        if (C2_OK != res) {
                            return C2_CORRUPTED;
                        }

                        block_pool.push_back(*new_block);

                        std::unique_ptr<native_handle_t, decltype(hndl_deleter)> new_hndl(
                            android::UnwrapNativeCodec2GrallocHandle(new_block->handle()), hndl_deleter);

                        if(new_hndl == nullptr) {
                            return C2_NO_MEMORY;
                        }

                        it = m_surfaces.end();

                        if (C2_OK != MfxGrallocInstance::getInstance()->GetBackingStore(new_hndl.get(), &id)) {
                            return C2_CORRUPTED;
                        }

                        it = m_surfaces.find(id);

                        if(C2_OK == res && (it == m_surfaces.end() || (it != m_surfaces.end() && !it->second->Data.Locked))) {
                             block_pool.clear();
                             found_surface = true;
                             if(it == m_surfaces.end()) {
                                 res = MfxC2FrameOut::Create(converter, std::move(new_block), m_mfxVideoParams.mfx.FrameInfo, frame_out, new_hndl.get());
                                 if (C2_OK != res) {
                                     return C2_CORRUPTED;
                                 }
                                 m_surfaces.emplace(id, frame_out->GetMfxFrameSurface());
                             } else {
                                 *frame_out = MfxC2FrameOut(std::move(new_block), it->second);
                             }
                        }
                    }
                }
            }
        } else {
            // Disable the optimization for 4K thumbnail generation
            if (0/*m_mfxVideoParams.mfx.FrameInfo.Width >= WIDTH_2K || m_mfxVideoParams.mfx.FrameInfo.Height >= HEIGHT_2K*/) {
                // Thumbnail generation for 4K/8K video
                if (m_surfacePool.size() < m_surfaceNum) {
                    res = MfxC2FrameOut::Create(out_block, m_mfxVideoParams.mfx.FrameInfo, TIMEOUT_NS, frame_out);
                    if (C2_OK != res) {
                        break;
                    }

                    m_blocks.push_back(std::make_pair(frame_out->GetMfxFrameSurface().get(), out_block));
                    m_surfacePool.push_back(frame_out->GetMfxFrameSurface());
                } else {
                    auto it = m_surfacePool.begin();
                    for(auto& mfx_frame: m_surfacePool) {
                        // Check if there is avaiable surface in the pool
                        if (!mfx_frame->Data.Locked) {
                            auto blk = m_blocks.begin();
                            for (; blk != m_blocks.end(); blk++) {
                                // Retrieve the corresponding block
                                if (blk->first == mfx_frame.get()) {
                                    std::shared_ptr<C2GraphicBlock> block = blk->second;
                                    *frame_out = MfxC2FrameOut(std::move(block), mfx_frame);
                                    break;
                                }
                            }
                            if (blk != m_blocks.end()) break;
                       }
                    }

                    if (it == m_surfacePool.end()) {
                        ALOGE("Cannot find available surface");
                        res = C2_BAD_VALUE;
                    }
                }
            } else {
                // small resolution video playback with system memory
                if(!vpp_conversion) {
                    res = MfxC2FrameOut::Create(std::move(out_block), m_mfxVideoParams.mfx.FrameInfo, TIMEOUT_NS, frame_out);
                } else {
                    mfxFrameInfo frame_info = m_mfxVideoParams.mfx.FrameInfo;
                    frame_info.FourCC = MFX_FOURCC_NV12;
                    res = MfxC2FrameOut::Create(std::move(out_block), frame_info, TIMEOUT_NS, frame_out);
                }
            }

            if (C2_OK != res) {
                break;
            }
        }

    } while (C2_TIMED_OUT == res);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

bool MfxC2DecoderComponent::IsDuplicatedTimeStamp(uint64_t timestamp)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I64(timestamp);

    bool bDuplicated = false;

    auto duplicate = find_if(m_duplicatedTimeStamp.begin(), m_duplicatedTimeStamp.end(),
        [timestamp] (const auto &item) {
            return item.first == timestamp;
    });

    if (duplicate != m_duplicatedTimeStamp.end()) {
        bDuplicated = true;
        MFX_DEBUG_TRACE_STREAM("Potentional error: Found duplicated timestamp: "
                        << duplicate->first);
    }

    return bDuplicated;
}

bool MfxC2DecoderComponent::IsPartialFrame(uint64_t frame_index)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I64(frame_index);

    bool bDuplicated = false;

    auto duplicate = find_if(m_duplicatedTimeStamp.begin(), m_duplicatedTimeStamp.end(),
        [frame_index] (const auto &item) {
            return item.second == frame_index;
    });

    if (duplicate != m_duplicatedTimeStamp.end()) {
        bDuplicated = true;
        MFX_DEBUG_TRACE_STREAM("Potentional error: Found duplicated timestamp: "
                        << duplicate->first);
    }

    return bDuplicated;
}

void MfxC2DecoderComponent::EmptyReadViews(uint64_t timestamp, uint64_t frame_index)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I64(timestamp);
    MFX_DEBUG_TRACE_I64(frame_index);

    if (!IsDuplicatedTimeStamp(timestamp)) {
        ReleaseReadViews(frame_index);
        return;
    }

    auto it = m_duplicatedTimeStamp.begin();
    for (; it != m_duplicatedTimeStamp.end(); it++) {
        if (it->first < timestamp) {
           ReleaseReadViews(it->second);
        }
    }
}

void MfxC2DecoderComponent::ReleaseReadViews(uint64_t incoming_frame_index)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_I64(incoming_frame_index);

    std::unique_ptr<C2ReadView> read_view;
    std::lock_guard<std::mutex> lock(m_readViewMutex);

    auto it = m_readViews.find(incoming_frame_index);
    if (it != m_readViews.end()) {
        read_view = std::move(it->second);
        read_view.reset();
        m_readViews.erase(it);
        MFX_DEBUG_TRACE_STREAM("release read_view with " <<
            NAMED(incoming_frame_index));
    }
}

void MfxC2DecoderComponent::DoWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    if (!work) return;

    if (m_bFlushing) {
        m_flushedWorks.push_back(std::move(work));
        return;
    }

    c2_status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    const auto incoming_frame_index = work->input.ordinal.frameIndex;
    const auto incoming_timestamp = work->input.ordinal.timestamp;
    const auto incoming_flags = work->input.flags;

    MFX_DEBUG_TRACE_STREAM("work: " << work.get() << "; index: " << incoming_frame_index.peeku() <<
        "; timestamp: " << incoming_timestamp.peeku() <<
        " flags: " << std::hex << incoming_flags);

    bool expect_output = false;
    bool flushing = false;
    bool codecConfig = ((incoming_flags & C2FrameData::FLAG_CODEC_CONFIG) != 0);
    // Av1 and VP9 don't need the bs which flag is config.
    if (codecConfig && (DECODER_AV1 == m_decoderType || DECODER_VP9 == m_decoderType)) {
        FillEmptyWork(std::move(work), C2_OK);
        if (true == m_bInitialized) {
            mfxStatus format_change_sts = HandleFormatChange();
            MFX_DEBUG_TRACE__mfxStatus(format_change_sts);
            mfx_sts = format_change_sts;
            if (MFX_ERR_NONE != mfx_sts) {
                FreeDecoder();
            }
        }
        return;
    } else if (DECODER_AV1 == m_decoderType && m_c2Bitstream->IsInReset()) {
        if (true == m_bInitialized) {
            mfxStatus format_change_sts = HandleFormatChange();
            MFX_DEBUG_TRACE__mfxStatus(format_change_sts);
            mfx_sts = format_change_sts;
            if (MFX_ERR_NONE != mfx_sts) {
                FreeDecoder();
            }
        }
    }

    bool encounterResolutionChanged = false;
    do {
        std::unique_ptr<C2ReadView> read_view;
        res = m_c2Bitstream->AppendFrame(work->input, TIMEOUT_NS, &read_view, codecConfig);
        if (C2_OK != res) break;

        {
            std::lock_guard<std::mutex> lock(m_readViewMutex);
            m_readViews.emplace(incoming_frame_index.peeku(), std::move(read_view));
            MFX_DEBUG_TRACE_STREAM("emplace readview with " << NAMED(incoming_frame_index.peeku())
                << NAMED(incoming_timestamp.peeku()));
            MFX_DEBUG_TRACE_I32(m_readViews.size());
        }

        if (work->input.buffers.size() == 0) break;

        PushPending(std::move(work));

        if (!m_c2Allocator) {
            res = GetCodec2BlockPool(m_outputPoolId,
                shared_from_this(), &m_c2Allocator);
            if (res != C2_OK) break;
        }
        // loop repeats DecodeFrame on the same frame
        // if DecodeFrame returns error which is repairable, like resolution change
        bool resolution_change = false;
	bool need_reset = false;
        do {
            if (!m_bInitialized) {
                mfx_sts = InitDecoder(m_c2Allocator);
                if(MFX_ERR_NONE != mfx_sts) {
                    MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                    if (MFX_ERR_MORE_DATA == mfx_sts) {
                        mfx_sts = MFX_ERR_NONE; // not enough data for InitDecoder should not cause an error
                    }
                    res = MfxStatusToC2(mfx_sts);
                    break;
                }
                if (!m_bInitialized) {
                    MFX_DEBUG_TRACE_MSG("Cannot initialize mfx decoder");
                    res = C2_BAD_VALUE;
                    break;
                }
            }

            if (!m_bSetHdrStatic) UpdateHdrStaticInfo();

            mfxBitstream *bs = m_c2Bitstream->GetFrameConstructor()->GetMfxBitstream().get();

            {
                std::lock_guard<std::mutex> lock(m_dump_input_lock);
                if (m_inputWriter.get() != NULL && bs != NULL && bs->DataLength > 0) {
                    m_inputWriter->Write(bs->Data, bs->DataLength);
                }
            }

            MfxC2FrameOut frame_out;
            do {
                // check bitsream is empty
                if (bs && bs->DataLength == 0) {
                    mfx_sts = MFX_ERR_MORE_DATA;
                    break;
                }

                res = AllocateFrame(&frame_out);
                if (C2_OK != res) break;

                mfx_sts = DecodeFrame(bs, std::move(frame_out), &flushing, &expect_output);
            } while (mfx_sts == MFX_ERR_NONE || mfx_sts == MFX_ERR_MORE_SURFACE);

            if (MFX_ERR_MORE_DATA == mfx_sts) {
                mfx_sts = MFX_ERR_NONE; // valid result of DecodeFrame
            }

            resolution_change = (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mfx_sts) ||
		    (MFX_WRN_VIDEO_PARAM_CHANGED == mfx_sts);
	    need_reset = (MFX_WRN_VIDEO_PARAM_CHANGED == mfx_sts);
            if (resolution_change) {
                encounterResolutionChanged = true;

                frame_out = MfxC2FrameOut(); // release the frame to be used in Drain

                Drain(nullptr);

                // Clear up all queue of works after drain except last work
                // which caused resolution change and should be used again.
                {
                    std::lock_guard<std::mutex> lock(m_pendingWorksMutex);
                    auto it = m_pendingWorks.begin();
                    while (it != m_pendingWorks.end()) {
                        if (it->first != incoming_frame_index) {
                            MFX_DEBUG_TRACE_STREAM("Work removed: " << NAMED(it->second->input.ordinal.frameIndex.peeku()));
                            NotifyWorkDone(std::move(it->second), C2_NOT_FOUND);
                            it = m_pendingWorks.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                mfxStatus format_change_sts = HandleFormatChange(need_reset);
                MFX_DEBUG_TRACE__mfxStatus(format_change_sts);
                mfx_sts = format_change_sts;
                if (MFX_ERR_NONE != mfx_sts) {
                    FreeDecoder();
                }
            }

        } while (resolution_change); // try again as it is a resolution change

        if (C2_OK != res) break; // if loop above was interrupted by C2 error

        if (MFX_ERR_NONE != mfx_sts) {
            MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
            res = MfxStatusToC2(mfx_sts);
            break;
        }

        res = m_c2Bitstream->Unload();
        if (C2_OK != res) break;

    } while(false); // fake loop to have a cleanup point there

    bool incomplete_frame =
        (incoming_flags & (C2FrameData::FLAG_INCOMPLETE | C2FrameData::FLAG_CODEC_CONFIG)) != 0;

    // Sometimes the frame is split to several buffers with the same timestamp.
    // If the current input bitstream causes a change in resolution,
    // the corresponding decoded frame must be returned to the framework in the 'WaitWork' thread,
    // rather than returning an empty output in the 'DoWork' thread.
    if (!encounterResolutionChanged) {
        incomplete_frame |= IsPartialFrame(incoming_frame_index.peeku());
    }

    // notify listener in case of failure or empty output
    if (C2_OK != res || !expect_output || incomplete_frame || flushing) {
        if (!work) {
            std::lock_guard<std::mutex> lock(m_pendingWorksMutex);
            auto it = m_pendingWorks.find(incoming_frame_index);
            if (it != m_pendingWorks.end()) {
                work = std::move(it->second);
                m_pendingWorks.erase(it);
            } else {
                MFX_DEBUG_TRACE_STREAM("Not found C2Work, index = " << incoming_frame_index.peeku());
                // If not found, it might be removed by WaitWork. We don't need to return an error.
                // FatalError(C2_CORRUPTED);
            }
        }
        if (work) {
            if (flushing) {
                m_flushedWorks.push_back(std::move(work));
            } else {
                FillEmptyWork(std::move(work), res);
            }
        }
    }
}

void MfxC2DecoderComponent::FillEmptyWork(std::unique_ptr<C2Work>&& work, c2_status_t res)
{
    MFX_DEBUG_TRACE_FUNC;

    uint32_t flags = 0;
    // Pass end of stream flag only
    if (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) {
        flags |= C2FrameData::FLAG_END_OF_STREAM;
        MFX_DEBUG_TRACE_MSG("signalling eos");
    }

    std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
    worklet->output.flags = (C2FrameData::flags_t)flags;
    // No output buffers
    worklet->output.buffers.clear();
    worklet->output.ordinal = work->input.ordinal;

    NotifyWorkDone(std::move(work), res);
}


void MfxC2DecoderComponent::Drain(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_sts = MFX_ERR_NONE;

    if (m_bInitialized) {
        do {
            if (m_bFlushing) {
                if (work) {
                    m_flushedWorks.push_back(std::move(work));
                }
                break;
            }

            MfxC2FrameOut frame_out;
            c2_status_t c2_sts = AllocateFrame(&frame_out);
            if (C2_OK != c2_sts) break; // no output allocated, no sense in calling DecodeFrame

            bool expect_output{};
            bool flushing{};
            mfx_sts = DecodeFrame(nullptr, std::move(frame_out), &flushing, &expect_output);
            if (flushing) {
                if (work) {
                    m_flushedWorks.push_back(std::move(work));
                }
                break;
            }
        // exit cycle if MFX_ERR_MORE_DATA or critical error happens
        } while (MFX_ERR_NONE == mfx_sts || MFX_ERR_MORE_SURFACE == mfx_sts);

        // eos work, should be sent after last work returned
        if (work) {
            m_waitingQueue.Push([work = std::move(work), this]() mutable {
                FillEmptyWork(std::move(work), C2_OK);
            });
        }

        const auto timeout = std::chrono::seconds(10);
        std::unique_lock<std::mutex> lock(m_devBusyMutex);
        bool cv_res =
            m_devBusyCond.wait_for(lock, timeout, [this] { return m_uSyncedPointsCount == 0; } );
        if (!cv_res) {
            MFX_DEBUG_TRACE_MSG("Timeout on drain completion");
        }
    } else {
        if (work) {
            FillEmptyWork(std::move(work), C2_OK);
        }
    }
}

const u_int16_t ytile_width = 16;
const u_int16_t ytile_height = 32;
static void one_ytiled_to_linear(const unsigned char *src, char *dst, u_int16_t x, u_int16_t y,
                          u_int16_t width, u_int16_t height, uint32_t offset)
{
    // x and y follow linear
    u_int32_t count = x + y * width / ytile_width;

    for (int j = 0; j < ytile_width * ytile_height; j += ytile_width) {
        memcpy(dst + offset + width * ytile_height * y + width * j / ytile_width + x * ytile_width,
                src + offset + j + ytile_width * ytile_height * count, ytile_width);
    }
}

static void* ytiled_to_linear(uint32_t total_size, uint32_t y_size, uint32_t stride,
                       const unsigned char *src)
{
       char* dst = (char*)malloc(total_size * 2);
       if (NULL == dst) return NULL;

       memset(dst, 0, total_size * 2);

       u_int16_t height = y_size / stride;

       // Y
       u_int16_t y_hcount =  height / ytile_height + (height % ytile_height != 0);
       for (u_int16_t x = 0; x < stride / ytile_width; x ++) {
               for (u_int16_t y = 0; y < y_hcount; y ++) {
                       one_ytiled_to_linear(src, dst, x, y, stride, height, 0);
               }
       }

       // UV
       u_int16_t uv_hcount =  (height / ytile_height / 2) + (height % (ytile_height * 2) != 0);
       for (u_int16_t x = 0; x < stride / ytile_width; x ++) {
               for (u_int16_t y = 0; y < uv_hcount; y ++) {
                       one_ytiled_to_linear(src, dst, x, y, stride, height / 2, y_size);
               }
       }

       return dst;
}

static bool NeedDumpOutput(uint32_t& dump_count,
                           std::unique_ptr<BinaryWriter>& output_writer,
                           std::shared_ptr<C2ComponentNameSetting> decoder_name,
                           uint32_t& file_num)
{
    MFX_DEBUG_TRACE_FUNC;
    bool result = false;
    char* value = new char[20];
    unsigned int frame_number = 0;

    if(property_get(DECODER_DUMP_OUTPUT_KEY, value, NULL) > 0) {
        std::stringstream strValue;
        strValue << value;
        strValue >> frame_number;
    }

    if (dump_count) {
        delete[] value;
        result = true;
    } else {
        delete[] value;
        if (frame_number > 0) {
            property_set(DECODER_DUMP_OUTPUT_KEY, NULL);

            std::ostringstream file_name;
            file_name << decoder_name->m.value << "frame_" << std::to_string(file_num) << ".yuv";

            MFX_DEBUG_TRACE_STREAM("Decoder output dump is started to " <<
                MFX_C2_DUMP_DIR << "/" << MFX_C2_DUMP_DECODER_SUB_DIR << "/"
                <<MFX_C2_DUMP_OUTPUT_SUB_DIR << "/" << file_name.str());

            output_writer = std::make_unique<BinaryWriter>(MFX_C2_DUMP_DIR,
                                 std::vector<std::string>({MFX_C2_DUMP_DECODER_SUB_DIR,
                                 MFX_C2_DUMP_OUTPUT_SUB_DIR}), file_name.str());

	    if(output_writer) {
                dump_count = frame_number;
		file_num ++;
                MFX_DEBUG_TRACE_PRINTF("--------triggered to dump %d buffers---------", frame_number);
		result = true;
	    } else {
                MFX_DEBUG_TRACE_PRINTF("create output writer failed");
                result = false;
            }
        }
    }
    return result;
}

static void DumpOutput(std::shared_ptr<C2GraphicBlock> block,
	               std::shared_ptr<mfxFrameSurface1> mfx_surface,
	               uint32_t& dump_count, const mfxVideoParam& mfxVideoParams,
	               std::unique_ptr<BinaryWriter>& output_writer)
{
    MFX_DEBUG_TRACE_FUNC;

    if (!block || !mfx_surface) return;

    static std::ofstream dump_stream;
    const C2GraphicView& output_view = block->map().get();

    if (dump_count) {
        const unsigned char* srcY = (const unsigned char*)output_view.data()[C2PlanarLayout::PLANE_Y];
        const unsigned char* srcUV = (const unsigned char*)output_view.data()[C2PlanarLayout::PLANE_U];

        if (output_writer && srcY != NULL && srcUV != NULL) {
            if(MFX_IOPATTERN_OUT_VIDEO_MEMORY == mfxVideoParams.IOPattern) {
                const uint32_t align_width = 128;
                const uint32_t align_height = 32;

                uint32_t surface_width = mfx_surface->Info.Width % align_width == 0? mfx_surface->Info.Width :
                        (mfx_surface->Info.Width / align_width + 1) * align_width;
                uint32_t surface_height = mfx_surface->Info.Height % align_height == 0? mfx_surface->Info.Height :
                        (mfx_surface->Info.Height / align_height + 1) * align_height;

                uint32_t pix_len = (MFX_FOURCC_P010 == mfx_surface->Info.FourCC) ? 2 : 1;

                uint32_t total_size = surface_width * pix_len * surface_height * 3 / 2;
                uint32_t y_size = surface_width * pix_len * surface_height;
                uint32_t stride = surface_width * pix_len;

                unsigned char* srcY_linear = (unsigned char *)ytiled_to_linear(total_size,
                                                 y_size, stride, (const unsigned char *)srcY);

                if (NULL != srcY_linear) {

                    output_writer->Write(srcY_linear, total_size);
                    free(srcY_linear);
                    srcY_linear = NULL;

                    dump_count --;

                    MFX_DEBUG_TRACE_PRINTF("######## dumping #%d to last decoded buffer in size: %dx%d ########",
                            dump_count, surface_width, surface_height);
                }
            } else { // IOPattern is system memory
                if (NULL != srcY && NULL != srcUV) {
                    uint32_t pitchLow = mfx_surface->Data.PitchLow;
                    uint32_t cropH = mfxVideoParams.mfx.FrameInfo.CropH;
                    output_writer->Write(srcY, pitchLow * cropH);
                    output_writer->Write(srcUV, (pitchLow * cropH) / 2);

                    uint32_t dump_width = (MFX_FOURCC_P010 == mfx_surface->Info.FourCC) ?
                            pitchLow / 2 : pitchLow;

                    dump_count--;

                    MFX_DEBUG_TRACE_PRINTF("######## dumping #%d to last decoded buffer in size: %dx%d  ########",
                            dump_count, dump_width, mfxVideoParams.mfx.FrameInfo.CropH);

                }
            }
        }
    }

    if (dump_count == 0 && output_writer.get() != NULL) {
	output_writer->Close();
	output_writer.reset();

        MFX_DEBUG_TRACE_MSG("Output writer reset");
    }

    return;
}

void MfxC2DecoderComponent::WaitWork(MfxC2FrameOut&& frame_out, mfxSyncPoint sync_point)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    mfxStatus mfx_res = MFX_ERR_NONE;

    {
#ifdef USE_ONEVPL
        mfx_res = MFXVideoCORE_SyncOperation(m_mfxSession, sync_point, MFX_TIMEOUT_INFINITE);
#else
        mfx_res = m_mfxSession.SyncOperation(sync_point, MFX_TIMEOUT_INFINITE);
#endif
        if (MFX_ERR_NONE != mfx_res) {
            MFX_DEBUG_TRACE_MSG("SyncOperation failed");
            MFX_DEBUG_TRACE__mfxStatus(mfx_res);
            res = MfxStatusToC2(mfx_res);
        }
    }

    std::shared_ptr<mfxFrameSurface1> mfx_surface = frame_out.GetMfxFrameSurface();
    MFX_DEBUG_TRACE_I32(mfx_surface->Data.Locked);
    MFX_DEBUG_TRACE_I64(mfx_surface->Data.TimeStamp);
    MFX_DEBUG_TRACE_I32(mfx_surface->Data.FrameOrder);
    MFX_DEBUG_TRACE_I64(mfx_surface->Data.PitchLow);
    MFX_DEBUG_TRACE_I32(mfx_surface->Info.CropW);
    MFX_DEBUG_TRACE_I32(mfx_surface->Info.CropH);
    MFX_DEBUG_TRACE_I32(m_mfxVideoParams.mfx.FrameInfo.CropW);
    MFX_DEBUG_TRACE_I32(m_mfxVideoParams.mfx.FrameInfo.CropH);

    MfxC2FrameOut frameOutVpp;
    std::shared_ptr<mfxFrameSurface1> mfx_surface_vpp;
    if (m_vppConversion) {
        res = AllocateFrame(&frameOutVpp, true);
        if (res == C2_OK) {
            mfx_surface_vpp = frameOutVpp.GetMfxFrameSurface();
            if (mfx_surface_vpp.get()) {
                mfxSyncPoint syncp;
                mfx_res = m_vpp->RunFrameVPPAsync(mfx_surface.get(), mfx_surface_vpp.get(), NULL, &syncp);
                if (MFX_ERR_NONE == mfx_res)
    #ifdef USE_ONEVPL
                    mfx_res = MFXVideoCORE_SyncOperation(m_mfxSession, syncp, MFX_TIMEOUT_INFINITE);
    #else
                    mfx_res = m_pSession->SyncOperation(syncp, MFX_TIMEOUT_INFINITE);
    #endif
            }
        }
    }

    uint64_t ready_timestamp = mfx_surface->Data.TimeStamp;

    std::unique_ptr<C2Work> work;
    std::unique_ptr<C2ReadView> read_view;

    {
        std::lock_guard<std::mutex> lock(m_pendingWorksMutex);

        auto it = find_if(m_pendingWorks.begin(), m_pendingWorks.end(), [ready_timestamp] (const auto &item) {
            // In some cases, operations in oneVPL may result in timestamp gaps.
            // To ensure that we correctly identify the work, we replicate the same operations as performed in oneVPL.
            return item.second->input.ordinal.timestamp.peeku() == ready_timestamp ||
                   GetMfxTimeStamp(GetUmcTimeStamp(item.second->input.ordinal.timestamp.peeku())) == ready_timestamp;
        });

        if (it != m_pendingWorks.end()) {
            work = std::move(it->second);
            MFX_DEBUG_TRACE_STREAM("Work removed: " << NAMED(work->input.ordinal.frameIndex.peeku()));
            m_pendingWorks.erase(it);
        }
    }

    if (work) {
        const auto frame_timestamp = work->input.ordinal.timestamp;
        const auto frame_index = work->input.ordinal.frameIndex;

        // Release input buffers
        EmptyReadViews(frame_timestamp.peeku(), frame_index.peeku());
    }

    do {

        //C2Event event; // not supported yet, left for future use
        //event.fire(); // pre-fire event as output buffer is ready to use

        C2Rect rect;
        if(!m_vppConversion) {
            rect = C2Rect(mfx_surface->Info.CropW, mfx_surface->Info.CropH)
                          .at(mfx_surface->Info.CropX, mfx_surface->Info.CropY);
        } else {
            if (mfx_surface_vpp.get() == nullptr) {
                res = C2_CORRUPTED;
                break;
            }
            rect = C2Rect(mfx_surface_vpp->Info.CropW, mfx_surface_vpp->Info.CropH)
                          .at(mfx_surface_vpp->Info.CropX, mfx_surface_vpp->Info.CropY);
        }

        {
            // Update frame format description to be returned by Query method
            // through parameters C2StreamPictureSizeInfo and C2StreamCropRectInfo.
            // Although updated frame format is available right after DecodeFrameAsync call,
            // getting update there doesn't give any improvement in terms of mutex sync,
            // as m_initDecoderMutex is not locked there.

            // Also parameter update here is easily tested by comparison parameter with output in onWorkDone callback.
            // If parameters update is done after DecodeFrameAsync call
            // then it becomes not synchronized with output and input,
            // looks random from client side and cannot be tested.
            std::lock_guard<std::mutex> lock(m_initDecoderMutex);
            m_mfxVideoParams.mfx.FrameInfo = mfx_surface->Info;
        }

        std::shared_ptr<C2GraphicBlock> block;
        if(!m_vppConversion) {
            block = frame_out.GetC2GraphicBlock();
        } else {
            block = frameOutVpp.GetC2GraphicBlock();
        }
        if (!block) {
            res = C2_CORRUPTED;
            break;
        }

        {
            std::lock_guard<std::mutex> lock(m_lockedSurfacesMutex);
            // If output is not Locked - we have to release it asap to not interfere with possible
            // frame mapping in onWorkDone callback,
            // if output is Locked - remove it temporarily from m_lockedSurfaces to avoid holding
            // m_lockedSurfacesMutex while long copy operation.
            m_lockedSurfaces.remove(frame_out);
        }

        if (mfx_surface->Data.Locked) {
            // if output is still locked, return it back to m_lockedSurfaces
            std::lock_guard<std::mutex> lock(m_lockedSurfacesMutex);
            m_lockedSurfaces.push_back(frame_out);
        }

        if (work) {

            std::shared_ptr<C2Buffer> out_buffer =
                CreateGraphicBuffer(std::move(block), rect);

            // set static hdr info
            out_buffer->setInfo(m_hdrStaticInfo);

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
            // Pass end of stream flag only.
            worklet->output.flags = (C2FrameData::flags_t)(work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
            worklet->output.ordinal = work->input.ordinal;
	        if (m_mfxVideoParams.mfx.FrameInfo.Width != m_size->width || m_mfxVideoParams.mfx.FrameInfo.Height != m_size->height) {
                MFX_DEBUG_TRACE_STREAM("find m_size different from m_mfxVideoParams, update width from " << m_size->width
                                        << " to " << m_mfxVideoParams.mfx.FrameInfo.Width << ", height from " << m_size->height
                                        << " to " << m_mfxVideoParams.mfx.FrameInfo.Height);
                m_size->width = m_mfxVideoParams.mfx.FrameInfo.Width;
                m_size->height = m_mfxVideoParams.mfx.FrameInfo.Height;
                C2StreamPictureSizeInfo::output new_size(0u, m_size->width, m_size->height);
                m_updatingC2Configures.push_back(C2Param::Copy(new_size));
            }

            // Update codec's configure
            for (int i = 0; i < m_updatingC2Configures.size(); i++) {
                worklet->output.configUpdate.push_back(std::move(m_updatingC2Configures[i]));
            }
            m_updatingC2Configures.clear();

	    {
                std::lock_guard<std::mutex> lock(m_dump_output_lock);

                if (NeedDumpOutput(m_dump_count, m_outputWriter, m_name, m_file_num)) {
                    DumpOutput(block, mfx_surface, m_dump_count, m_mfxVideoParams, m_outputWriter);
                }
	    }

            worklet->output.buffers.push_back(out_buffer);
            block = nullptr;
        }
    } while (false);
    // Release output frame before onWorkDone is called, release causes unmap for system memory.
    frame_out = MfxC2FrameOut();
    if(m_vppConversion) {
        frameOutVpp = MfxC2FrameOut();
    }

    if (work) {
        NotifyWorkDone(std::move(work), res);
    }

    {
      std::unique_lock<std::mutex> lock(m_devBusyMutex);
      --m_uSyncedPointsCount;
    }
    m_devBusyCond.notify_one();
}

void MfxC2DecoderComponent::PushPending(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(m_pendingWorksMutex);

    const auto incoming_frame_timestamp = work->input.ordinal.timestamp;
    auto duplicate = find_if(m_pendingWorks.begin(), m_pendingWorks.end(),
        [incoming_frame_timestamp] (const auto &item) {
            // When decoding the image, the timestamp is always 0, so it is excluded.
            return (item.second->input.ordinal.timestamp == incoming_frame_timestamp) && ((c2_cntr64_t)0 != incoming_frame_timestamp);
        });
    if (duplicate != m_pendingWorks.end()) {
        MFX_DEBUG_TRACE_STREAM("Potentional error: Found duplicated timestamp: "
                               << duplicate->second->input.ordinal.timestamp.peeku());

        uint64_t duplicated_timestamp = duplicate->second->input.ordinal.timestamp.peeku();
        if (!IsDuplicatedTimeStamp(duplicated_timestamp)) {
            m_duplicatedTimeStamp.push_back(std::make_pair(duplicate->second->input.ordinal.timestamp.peeku(),
                                               duplicate->second->input.ordinal.frameIndex.peeku()));
            MFX_DEBUG_TRACE_STREAM("record work with duplicated timestamp: " << duplicate->second->input.ordinal.timestamp.peeku() <<
                "; index: " << duplicate->second->input.ordinal.frameIndex.peeku());
        }
        m_duplicatedTimeStamp.push_back(std::make_pair(work->input.ordinal.timestamp.peeku(),
                                               work->input.ordinal.frameIndex.peeku()));
        MFX_DEBUG_TRACE_STREAM("record incoming work with duplicated timestamp: " << work->input.ordinal.timestamp.peeku() <<
            "; index: " << work->input.ordinal.frameIndex.peeku());
    }

    const auto incoming_frame_index = work->input.ordinal.frameIndex;
    auto it = m_pendingWorks.find(incoming_frame_index);
    if (it != m_pendingWorks.end()) { // Shouldn't be the same index there
        NotifyWorkDone(std::move(it->second), C2_CORRUPTED);
        MFX_DEBUG_TRACE_STREAM("Work removed: " << NAMED(it->second->input.ordinal.frameIndex.peeku()) <<
            NAMED(it->second->input.ordinal.timestamp.peeku()));
        m_pendingWorks.erase(it);
    }
    m_pendingWorks.emplace(incoming_frame_index, std::move(work));
}

c2_status_t MfxC2DecoderComponent::ValidateWork(const std::unique_ptr<C2Work>& work)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {

        if(work->worklets.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple worklets");
            res = C2_BAD_VALUE;
            break;
        }

        const std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

        if(worklet->output.buffers.size() != 0) {
            MFX_DEBUG_TRACE_STREAM(NAMED(worklet->output.buffers.size()));
            MFX_DEBUG_TRACE_MSG("Caller is not supposed to allocate output");
            res = C2_BAD_VALUE;
            break;
        }
    }
    while (false);

    return res;
}

c2_status_t MfxC2DecoderComponent::Queue(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& item : *items) {

        c2_status_t sts = ValidateWork(item);

        if (C2_OK == sts) {

            if (m_bEosReceived) { // All works following eos treated as errors.
                item->result = C2_BAD_VALUE;
                // Do this in working thread to follow Drain completion.
                m_workingQueue.Push( [work = std::move(item), this] () mutable {
                    PushPending(std::move(work));
                });
            } else {
                bool eos = (item->input.flags & C2FrameData::FLAG_END_OF_STREAM);
                bool empty = (item->input.buffers.size() == 0);
                if (eos) {
                    m_bEosReceived = true;
                }
                MFX_DEBUG_TRACE_I32(eos);
                MFX_DEBUG_TRACE_I32(empty);
                if (eos && empty) {
                    m_workingQueue.Push( [work = std::move(item), this] () mutable {
                        Drain(std::move(work));
                    });
                } else {
                    m_workingQueue.Push( [ work = std::move(item), this ] () mutable {
                        DoWork(std::move(work));
                    } );
                    if (eos) {
                        m_workingQueue.Push( [this] () { Drain(nullptr); } );
                    }
                }
            }
        } else {
            NotifyWorkDone(std::move(item), sts);
        }
    }

    return C2_OK;
}

c2_status_t MfxC2DecoderComponent::Flush(std::list<std::unique_ptr<C2Work>>* const flushedWork)
{
    MFX_DEBUG_TRACE_FUNC;

    m_bFlushing = true;

    m_workingQueue.Push([this] () {
        MFX_DEBUG_TRACE("DecoderReset");
        MFX_DEBUG_TRACE_STREAM(NAMED(m_mfxDecoder.get()));
        if (m_mfxDecoder) { // check if initialized already
            mfxStatus reset_sts = m_mfxDecoder->Reset(&m_mfxVideoParams);
            MFX_DEBUG_TRACE__mfxStatus(reset_sts);
        }

        if (m_c2Bitstream) {
            m_c2Bitstream->Reset();
        }
    } );

    // Wait to have no works queued between Queue and DoWork.
    m_workingQueue.WaitForEmpty();
    m_waitingQueue.WaitForEmpty();
    // Turn off flushing mode only after working/waiting queues did all the job,
    // given queue_nb should not be called by libstagefright simultaneously with
    // flush_sm, no threads read/write m_flushedWorks list, so it can be used here
    // without block.
    m_bFlushing = false;

    {
        std::lock_guard<std::mutex> lock(m_pendingWorksMutex);

        for (auto& item : m_pendingWorks) {
            m_flushedWorks.push_back(std::move(item.second));
        }
        m_pendingWorks.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_lockedSurfacesMutex);
        m_lockedSurfaces.clear();
    }

    {
        std::lock_guard<std::mutex> lock(m_readViewMutex);
        m_readViews.clear();
    }

    m_duplicatedTimeStamp.clear();

    FreeSurfaces();

    *flushedWork = std::move(m_flushedWorks);
    m_bEosReceived = false;

    return C2_OK;
}

void MfxC2DecoderComponent::UpdateHdrStaticInfo()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxPayload* pHdrSeiPayload = m_c2Bitstream->GetFrameConstructor()->GetSEI(MfxC2HEVCFrameConstructor::SEI_MASTERING_DISPLAY_COLOUR_VOLUME);

    const mfxU32 SEI_MASTERING_DISPLAY_COLOUR_VOLUME_SIZE = 24*8; // required size of data in bits

    if (nullptr != pHdrSeiPayload && pHdrSeiPayload->NumBit >= SEI_MASTERING_DISPLAY_COLOUR_VOLUME_SIZE && nullptr != pHdrSeiPayload->Data)
    {
        MFX_DEBUG_TRACE_MSG("Set HDR static info: SEI_MASTERING_DISPLAY_COLOUR_VOLUME");

        m_bSetHdrStatic = true;
        m_hdrStaticInfo->mastering.red.x = pHdrSeiPayload->Data[1] | (pHdrSeiPayload->Data[0] << 8);
        m_hdrStaticInfo->mastering.red.y = pHdrSeiPayload->Data[3] | (pHdrSeiPayload->Data[2] << 8);
        m_hdrStaticInfo->mastering.green.x = pHdrSeiPayload->Data[5] | (pHdrSeiPayload->Data[4] << 8);
        m_hdrStaticInfo->mastering.green.y = pHdrSeiPayload->Data[7] | (pHdrSeiPayload->Data[6] << 8);
        m_hdrStaticInfo->mastering.blue.x = pHdrSeiPayload->Data[9] | (pHdrSeiPayload->Data[8] << 8);
        m_hdrStaticInfo->mastering.blue.y = pHdrSeiPayload->Data[11] | (pHdrSeiPayload->Data[10] << 8);
        m_hdrStaticInfo->mastering.white.x = pHdrSeiPayload->Data[13] | (pHdrSeiPayload->Data[12] << 8);
        m_hdrStaticInfo->mastering.white.y = pHdrSeiPayload->Data[15] | (pHdrSeiPayload->Data[14] << 8);

        mfxU32 mMaxDisplayLuminanceX10000 = pHdrSeiPayload->Data[19] | (pHdrSeiPayload->Data[18] << 8) | (pHdrSeiPayload->Data[17] << 16) | (pHdrSeiPayload->Data[16] << 24);
        m_hdrStaticInfo->mastering.maxLuminance = mMaxDisplayLuminanceX10000 / 10000.0;

        mfxU32 mMinDisplayLuminanceX10000 = pHdrSeiPayload->Data[23] | (pHdrSeiPayload->Data[22] << 8) | (pHdrSeiPayload->Data[21] << 16) | (pHdrSeiPayload->Data[20] << 24);
        m_hdrStaticInfo->mastering.minLuminance = mMinDisplayLuminanceX10000 / 10000.0;
    }
    pHdrSeiPayload = m_c2Bitstream->GetFrameConstructor()->GetSEI(MfxC2HEVCFrameConstructor::SEI_CONTENT_LIGHT_LEVEL_INFO);

    const mfxU32 SEI_CONTENT_LIGHT_LEVEL_INFO_SIZE = 4*8; // required size of data in bits

    if (nullptr != pHdrSeiPayload && pHdrSeiPayload->NumBit >= SEI_CONTENT_LIGHT_LEVEL_INFO_SIZE && nullptr != pHdrSeiPayload->Data)
    {
        MFX_DEBUG_TRACE_MSG("Set HDR static info: SEI_CONTENT_LIGHT_LEVEL_INFO");

        m_bSetHdrStatic = true;
        m_hdrStaticInfo->maxCll = pHdrSeiPayload->Data[1] | (pHdrSeiPayload->Data[0] << 8);
        m_hdrStaticInfo->maxFall = pHdrSeiPayload->Data[3] | (pHdrSeiPayload->Data[2] << 8);
    }

    MFX_DEBUG_TRACE__hdrStaticInfo(m_hdrStaticInfo);
}

void MfxC2DecoderComponent::UpdateColorAspectsFromBitstream(const mfxExtVideoSignalInfo &signalInfo)
{
    MFX_DEBUG_TRACE_FUNC;

    if (DECODER_VP8 == m_decoderType) return;

    MFX_DEBUG_TRACE_I32(signalInfo.VideoFullRange);
    MFX_DEBUG_TRACE_I32(signalInfo.ColourPrimaries);
    MFX_DEBUG_TRACE_I32(signalInfo.TransferCharacteristics);
    MFX_DEBUG_TRACE_I32(signalInfo.MatrixCoefficients);

    android::ColorAspects bitstreamColorAspects;

    MfxC2ColorAspectsUtils::MfxToC2VideoRange(signalInfo.VideoFullRange, bitstreamColorAspects.mRange);
    MfxC2ColorAspectsUtils::MfxToC2ColourPrimaries(signalInfo.ColourPrimaries, bitstreamColorAspects.mPrimaries);
    MfxC2ColorAspectsUtils::MfxToC2TransferCharacteristics(signalInfo.TransferCharacteristics, bitstreamColorAspects.mTransfer);
    MfxC2ColorAspectsUtils::MfxToC2MatrixCoefficients(signalInfo.MatrixCoefficients, bitstreamColorAspects.mMatrixCoeffs);

    if (!C2Mapper::map(bitstreamColorAspects.mRange, &m_outColorAspects->range)) {
        m_outColorAspects->range = C2Color::RANGE_UNSPECIFIED;
    }
    if (!C2Mapper::map(bitstreamColorAspects.mPrimaries, &m_outColorAspects->primaries)) {
        m_outColorAspects->primaries = C2Color::PRIMARIES_UNSPECIFIED;
    }
    if (!C2Mapper::map(bitstreamColorAspects.mTransfer, &m_outColorAspects->transfer)) {
        m_outColorAspects->transfer = C2Color::TRANSFER_UNSPECIFIED;
    }
    if (!C2Mapper::map(bitstreamColorAspects.mMatrixCoeffs, &m_outColorAspects->matrix)) {
        m_outColorAspects->matrix = C2Color::MATRIX_UNSPECIFIED;
    }

    // VideoFormat == 5 indicates that video_format syntax element is not present
    if (signalInfo.VideoFormat == 5 && signalInfo.VideoFullRange == 0 && signalInfo.ColourDescriptionPresent == 0) {
        m_outColorAspects->range = C2Color::RANGE_UNSPECIFIED;
        m_outColorAspects->primaries = C2Color::PRIMARIES_UNSPECIFIED;
        m_outColorAspects->transfer = C2Color::TRANSFER_UNSPECIFIED;
        m_outColorAspects->matrix = C2Color::MATRIX_UNSPECIFIED;
    }

    // If the color aspects is not read through bitstream, we will use the default value which from framework.
    if (C2Color::RANGE_UNSPECIFIED == m_outColorAspects->range) {
        m_outColorAspects->range = m_defaultColorAspects->range;
    }
    if (C2Color::PRIMARIES_UNSPECIFIED == m_outColorAspects->primaries) {
        m_outColorAspects->primaries = m_defaultColorAspects->primaries;
    }
    if (C2Color::TRANSFER_UNSPECIFIED == m_outColorAspects->transfer) {
        m_outColorAspects->transfer = m_defaultColorAspects->transfer;
    }
    if (C2Color::MATRIX_UNSPECIFIED == m_outColorAspects->matrix) {
        m_outColorAspects->matrix = m_defaultColorAspects->matrix;
    }

    MFX_DEBUG_TRACE_I32(m_outColorAspects->range);
    MFX_DEBUG_TRACE_I32(m_outColorAspects->primaries);
    MFX_DEBUG_TRACE_I32(m_outColorAspects->transfer);
    MFX_DEBUG_TRACE_I32(m_outColorAspects->matrix);
    m_updatingC2Configures.push_back(C2Param::Copy(*m_outColorAspects));
}
