// Copyright (c) 2017-2021 Intel Corporation
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

#include "mfx_c2_encoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"
#include "C2PlatformSupport.h"
#include "mfx_gralloc_instance.h"

#include <limits>
#include <thread>
#include <chrono>
#include <iomanip>
#include <C2AllocatorGralloc.h>
#include <C2Config.h>
#include <Codec2Mapper.h>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_encoder_component"

const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;
const mfxU32 MFX_MAX_H264_FRAMERATE = 172;
const mfxU32 MFX_MAX_H265_FRAMERATE = 300;
const mfxU32 MFX_MAX_SURFACE_NUM = 10;

#define MAX_B_FRAMES 1

C2R MfxC2EncoderComponent::SizeSetter(bool mayBlock, const C2P<C2StreamPictureSizeInfo::input> &oldMe,
                        C2P<C2StreamPictureSizeInfo::input> &me) {
    
    MFX_DEBUG_TRACE_FUNC;
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

C2R MfxC2EncoderComponent::AVC_ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::output> &me) {
    (void)mayBlock;
    if (!me.F(me.v.profile).supportsAtAll(me.v.profile))
         me.set().profile = PROFILE_AVC_CONSTRAINED_BASELINE;
    if (!me.F(me.v.level).supportsAtAll(me.v.level))
        me.set().level = LEVEL_AVC_5_2;

    return C2R::Ok();
}


C2R MfxC2EncoderComponent::AV1_ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::output> &me) {
    (void)mayBlock;
    if (!me.F(me.v.profile).supportsAtAll(me.v.profile))
        me.set().profile = PROFILE_AV1_0;
    if (!me.F(me.v.level).supportsAtAll(me.v.level))
        me.set().level = LEVEL_AV1_7_3;

    return C2R::Ok();
}
C2R MfxC2EncoderComponent::HEVC_ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::output> &me) {
    (void)mayBlock;
    if (!me.F(me.v.profile).supportsAtAll(me.v.profile))
        me.set().profile = PROFILE_HEVC_MAIN;
    if (!me.F(me.v.level).supportsAtAll(me.v.level))
        me.set().level = LEVEL_HEVC_MAIN_5_1;
    
    return C2R::Ok();
}
C2R MfxC2EncoderComponent::VP9_ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::output> &me) {
    (void)mayBlock;
    if (!me.F(me.v.profile).supportsAtAll(me.v.profile))
        me.set().profile = PROFILE_VP9_0;
    if (!me.F(me.v.level).supportsAtAll(me.v.level))
        me.set().level = LEVEL_VP9_5;
    
    return C2R::Ok();
}

C2R MfxC2EncoderComponent::BitrateSetter(bool mayBlock, C2P<C2StreamBitrateInfo::output> &me) {
    (void)mayBlock;
    C2R res = C2R::Ok();
    if (me.v.value <= 4096) {
        me.set().value = 4096;
    }
    return res;
}

C2R MfxC2EncoderComponent::GopSetter(bool mayBlock, C2P<C2StreamGopTuning::output> &me) {
    (void)mayBlock;
    for (size_t i = 0; i < me.v.flexCount(); ++i) {
        const C2GopLayerStruct &layer = me.v.m.values[0];
        if (layer.type_ == C2Config::picture_type_t(C2Config::P_FRAME | C2Config::B_FRAME)
                && layer.count > MAX_B_FRAMES) {
            me.set().m.values[i].count = MAX_B_FRAMES;
        }
    }
    return C2R::Ok();
}

C2R MfxC2EncoderComponent::IntraRefreshSetter(bool mayBlock, C2P<C2StreamIntraRefreshTuning::output> &me) {
    (void)mayBlock;
    C2R res = C2R::Ok();
    if (me.v.period < 1) {
        me.set().mode = C2Config::INTRA_REFRESH_DISABLED;
        me.set().period = 0;
    } else {
        // only support arbitrary mode (cyclic in our case)
        me.set().mode = C2Config::INTRA_REFRESH_ARBITRARY;
    }
    return res;
}

C2R MfxC2EncoderComponent::ColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::input> &me) {
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

C2R MfxC2EncoderComponent::CodedColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::output> &me,
                                    const C2P<C2StreamColorAspectsInfo::input> &coded) {
    (void)mayBlock;
    me.set().range = coded.v.range;
    me.set().primaries = coded.v.primaries;
    me.set().transfer = coded.v.transfer;
    me.set().matrix = coded.v.matrix;
    return C2R::Ok();
}

std::unique_ptr<mfxEncodeCtrl> EncoderControl::AcquireEncodeCtrl()
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<mfxEncodeCtrl> res;

    if (nullptr != m_ctrlOnce) {
        res = std::move(m_ctrlOnce);
    } // otherwise return nullptr
    return res;
}

void EncoderControl::Modify(ModifyFunction& function)
{
    MFX_DEBUG_TRACE_FUNC;

    // modify ctrl_once, create if null
    if (nullptr == m_ctrlOnce) {
        m_ctrlOnce = std::make_unique<mfxEncodeCtrl>();
    }
    function(m_ctrlOnce.get());
}

MfxC2EncoderComponent::MfxC2EncoderComponent(const C2String name, const CreateConfig& config,
    std::shared_ptr<C2ReflectorHelper> reflector, EncoderType encoder_type) :
        MfxC2Component(name, config, std::move(reflector)),
        m_encoderType(encoder_type),
#ifdef USE_ONEVPL
        m_mfxSession(nullptr),
        m_mfxLoader(nullptr),
#endif
        m_uSyncedPointsCount(0),
        m_encSrfPool(nullptr),
        m_encOutBuf(nullptr),
        m_encSrfNum(MFX_MAX_SURFACE_NUM),
        m_bVppDetermined(false),
        m_inputVppType(CONVERT_NONE)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_U32(m_inputVppType);

    const unsigned int SINGLE_STREAM_ID = 0u;
    uint32_t MIN_W = 176;
    uint32_t MIN_H = 144;
    uint32_t MAX_W = 4096;
    uint32_t MAX_H = 4096;
    getMaxMinResolutionSupported(&MIN_W, &MIN_H, &MAX_W, &MAX_H);

    addParameter(
        DefineParam(m_kind, C2_PARAMKEY_COMPONENT_KIND)
        .withConstValue(new C2ComponentKindSetting(C2Component::KIND_ENCODER))
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
        DefineParam(m_size, C2_PARAMKEY_PICTURE_SIZE)
        .withDefault(new C2StreamPictureSizeInfo::input(SINGLE_STREAM_ID, MIN_W, MIN_H))
        .withFields({
            C2F(m_size, width).inRange(MIN_W, MAX_W, 2),
            C2F(m_size, height).inRange(MIN_H, MAX_H, 2),
        })
        .withSetter(SizeSetter)
        .build());

    addParameter(
        DefineParam(m_inputMediaType, C2_PARAMKEY_INPUT_MEDIA_TYPE)
        .withConstValue(AllocSharedString<C2PortMediaTypeSetting::input>("video/raw"))
        .build());
    
    addParameter(
        DefineParam(m_inputFormat, C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE)
        .withConstValue(new C2StreamBufferTypeSetting::input(
                SINGLE_STREAM_ID, C2BufferData::GRAPHIC))
        .build());
    
    addParameter(
        DefineParam(m_outputFormat, C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE)
        .withConstValue(new C2StreamBufferTypeSetting::output(
                SINGLE_STREAM_ID, C2BufferData::LINEAR))
        .build());

    addParameter(
        DefineParam(m_bitrateMode, C2_PARAMKEY_BITRATE_MODE)
        .withDefault(new C2StreamBitrateModeTuning::output(SINGLE_STREAM_ID, C2Config::BITRATE_VARIABLE))
        .withFields({
            C2F(m_bitrateMode, value).oneOf({
                C2Config::BITRATE_CONST,
                C2Config::BITRATE_VARIABLE,
                C2Config::BITRATE_IGNORE
            })
        })
        .withSetter(Setter<decltype(*m_bitrateMode)>::StrictValueWithNoDeps)
        .build());

    addParameter(
        DefineParam(m_bitrate, C2_PARAMKEY_BITRATE)
        .withDefault(new C2StreamBitrateInfo::output(SINGLE_STREAM_ID, 64000))
        .withFields({C2F(m_bitrate, value).inRange(4096, 40000000)})
        .withSetter(BitrateSetter)
        .build());

    addParameter(
        DefineParam(m_frameRate, C2_PARAMKEY_FRAME_RATE)
        .withDefault(new C2StreamFrameRateInfo::output(SINGLE_STREAM_ID, 1.))
        .withFields({C2F(m_frameRate, value).greaterThan(0.)})
        .withSetter(Setter<decltype(*m_frameRate)>::StrictValueWithNoDeps)
        .build());

    addParameter(
        DefineParam(m_gop, C2_PARAMKEY_GOP)
        .withDefault(C2StreamGopTuning::output::AllocShared(
                0 /* flexCount */, SINGLE_STREAM_ID /* stream */))
        .withFields({C2F(m_gop, m.values[0].type_).any(),
                        C2F(m_gop, m.values[0].count).any()})
        .withSetter(GopSetter)
        .build());
    
    addParameter(
        DefineParam(m_requestSync, C2_PARAMKEY_REQUEST_SYNC_FRAME)
        .withDefault(new C2StreamRequestSyncFrameTuning::output(SINGLE_STREAM_ID, C2_FALSE))
        .withFields({C2F(m_requestSync, value).oneOf({ C2_FALSE, C2_TRUE }) })
        .withSetter(Setter<decltype(*m_requestSync)>::NonStrictValueWithNoDeps)
        .build());

    addParameter(
        DefineParam(m_syncFramePeriod, C2_PARAMKEY_SYNC_FRAME_INTERVAL)
        .withDefault(new C2StreamSyncFrameIntervalTuning::output(SINGLE_STREAM_ID, 1000000))
        .withFields({C2F(m_syncFramePeriod, value).any()})
        .withSetter(Setter<decltype(*m_syncFramePeriod)>::StrictValueWithNoDeps)
        .build());

    switch(m_encoderType) {
        case ENCODER_AV1: {
            addParameter(
                DefineParam(m_outputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
                .withConstValue(C2PortMediaTypeSetting::output::AllocShared("video/av01"))
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::output(
                    SINGLE_STREAM_ID, PROFILE_AV1_0, LEVEL_AV1_7_3))
                .withFields({
                    C2F(m_profileLevel, C2ProfileLevelStruct::profile)
                        .oneOf({
                            PROFILE_AV1_0,
                            PROFILE_AV1_1,
                            PROFILE_AV1_2,
                        }),
                    C2F(m_profileLevel, C2ProfileLevelStruct::level)
                        .oneOf({
                            LEVEL_AV1_2, LEVEL_AV1_2_1, LEVEL_AV1_2_2, LEVEL_AV1_2_3,
                            LEVEL_AV1_3, LEVEL_AV1_3_1, LEVEL_AV1_3_2, LEVEL_AV1_3_3,
                            LEVEL_AV1_4, LEVEL_AV1_4_1, LEVEL_AV1_4_2, LEVEL_AV1_4_3,
                            LEVEL_AV1_5, LEVEL_AV1_5_1, LEVEL_AV1_5_2, LEVEL_AV1_5_3,
                            LEVEL_AV1_6, LEVEL_AV1_6_1, LEVEL_AV1_6_2, LEVEL_AV1_6_3,
                            LEVEL_AV1_7, LEVEL_AV1_7_1, LEVEL_AV1_7_2, LEVEL_AV1_7_3,
                        }),})
                .withSetter(AV1_ProfileLevelSetter)
                .build());
            break;
        };
        case ENCODER_H264: {
            addParameter(
                DefineParam(m_outputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
                .withConstValue(C2PortMediaTypeSetting::output::AllocShared("video/avc"))
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::output(
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
                .withSetter(AVC_ProfileLevelSetter)
                .build());
            break;
        };
        case ENCODER_H265: {
            addParameter(
                DefineParam(m_outputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
                .withConstValue(C2PortMediaTypeSetting::output::AllocShared("video/hevc"))
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::output(
                    SINGLE_STREAM_ID, PROFILE_HEVC_MAIN, LEVEL_HEVC_MAIN_6))
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
                            LEVEL_HEVC_MAIN_5, LEVEL_HEVC_MAIN_5_1,
                            LEVEL_HEVC_MAIN_5_2, LEVEL_HEVC_HIGH_4,
                            LEVEL_HEVC_HIGH_4_1, LEVEL_HEVC_HIGH_5,
                            LEVEL_HEVC_HIGH_5_1, LEVEL_HEVC_HIGH_5_2,
                            LEVEL_HEVC_MAIN_6, LEVEL_HEVC_MAIN_6_1,
                            LEVEL_HEVC_MAIN_6_2,
                        }),})
                .withSetter(HEVC_ProfileLevelSetter)
                .build());

            std::vector<uint32_t> supportedPixelFormats = {
                HAL_PIXEL_FORMAT_YCBCR_420_888,
                HAL_PIXEL_FORMAT_YCBCR_P010
            };

            addParameter(
                DefineParam(m_pixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
                .withDefault(new C2StreamPixelFormatInfo::input(
                                    0u, HAL_PIXEL_FORMAT_YCBCR_420_888))
                .withFields({C2F(m_pixelFormat, value).oneOf(supportedPixelFormats)})
                .withSetter((Setter<decltype(*m_pixelFormat)>::StrictValueWithNoDeps))
                .build());

            break;
        };
        case ENCODER_VP9: {
            addParameter(
                DefineParam(m_outputMediaType, C2_PARAMKEY_OUTPUT_MEDIA_TYPE)
                .withConstValue(C2PortMediaTypeSetting::output::AllocShared("video/x-vnd.on2.vp9"))
                .build());

            addParameter(DefineParam(m_profileLevel, C2_PARAMKEY_PROFILE_LEVEL)
                .withDefault(new C2StreamProfileLevelInfo::output(
                    SINGLE_STREAM_ID, PROFILE_VP9_0, LEVEL_VP9_5))
                .withFields({
                    C2F(m_profileLevel, C2ProfileLevelStruct::profile)
                        .oneOf({
                            PROFILE_VP9_0,
                            PROFILE_VP9_1,
                            // TODO: support 10-bit HDR
                            // PROFILE_VP9_2,
                            // PROFILE_VP9_3,
                        }),
                    C2F(m_profileLevel, C2ProfileLevelStruct::level)
                        .oneOf({
                            LEVEL_VP9_1, LEVEL_VP9_1_1,
                            LEVEL_VP9_2, LEVEL_VP9_2_1,
                            LEVEL_VP9_3, LEVEL_VP9_3_1,
                            LEVEL_VP9_4, LEVEL_VP9_4_1,
                            LEVEL_VP9_5,
                        }),})
                .withSetter(VP9_ProfileLevelSetter)
                .build());

            std::vector<uint32_t> supportedPixelFormats = {
                HAL_PIXEL_FORMAT_YCBCR_420_888,
                HAL_PIXEL_FORMAT_YCBCR_P010
            };

            addParameter(
                DefineParam(m_pixelFormat, C2_PARAMKEY_PIXEL_FORMAT)
                .withDefault(new C2StreamPixelFormatInfo::input(
                                    0u, HAL_PIXEL_FORMAT_YCBCR_420_888))
                .withFields({C2F(m_pixelFormat, value).oneOf(supportedPixelFormats)})
                .withSetter((Setter<decltype(*m_pixelFormat)>::StrictValueWithNoDeps))
                .build());

            break;
        };
        default: {
            // TODO: error
        };
    }

    addParameter(
        DefineParam(m_colorAspects, C2_PARAMKEY_COLOR_ASPECTS)
        .withDefault(new C2StreamColorAspectsInfo::input(
                SINGLE_STREAM_ID, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
        .withFields({
            C2F(m_colorAspects, range).inRange(
                        C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
            C2F(m_colorAspects, primaries).inRange(
                        C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
            C2F(m_colorAspects, transfer).inRange(
                        C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
            C2F(m_colorAspects, matrix).inRange(
                        C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
        })
        .withSetter(ColorAspectsSetter)
        .build());

    addParameter(
        DefineParam(m_codedColorAspects, C2_PARAMKEY_VUI_COLOR_ASPECTS)
        .withDefault(new C2StreamColorAspectsInfo::output(
                SINGLE_STREAM_ID, C2Color::RANGE_LIMITED, C2Color::PRIMARIES_UNSPECIFIED,
                C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED))
        .withFields({
            C2F(m_codedColorAspects, range).inRange(
                        C2Color::RANGE_UNSPECIFIED,     C2Color::RANGE_OTHER),
            C2F(m_codedColorAspects, primaries).inRange(
                        C2Color::PRIMARIES_UNSPECIFIED, C2Color::PRIMARIES_OTHER),
            C2F(m_codedColorAspects, transfer).inRange(
                        C2Color::TRANSFER_UNSPECIFIED,  C2Color::TRANSFER_OTHER),
            C2F(m_codedColorAspects, matrix).inRange(
                        C2Color::MATRIX_UNSPECIFIED,    C2Color::MATRIX_OTHER)
        })
        .withSetter(CodedColorAspectsSetter, m_colorAspects)
        .build());

    addParameter(
        DefineParam(m_intraRefresh, C2_PARAMKEY_INTRA_REFRESH)
        .withDefault(new C2StreamIntraRefreshTuning::output(
                0u, C2Config::INTRA_REFRESH_DISABLED, 0.))
        .withFields({
            C2F(m_intraRefresh, mode).oneOf({
                C2Config::INTRA_REFRESH_DISABLED, C2Config::INTRA_REFRESH_ARBITRARY }),
            C2F(m_intraRefresh, period).any()
        })
        .withSetter(IntraRefreshSetter)
        .build());
    // Color aspects
    //pr.RegisterParam<C2StreamColorAspectsInfo::input>(C2_PARAMKEY_COLOR_ASPECTS);
    //pr.RegisterParam<C2StreamColorAspectsInfo::output>(C2_PARAMKEY_VUI_COLOR_ASPECTS);

    m_colorAspects = std::make_shared<C2StreamColorAspectsInfo::input>();
    m_colorAspects->range = C2Color::RANGE_UNSPECIFIED;
    m_colorAspects->primaries = C2Color::PRIMARIES_UNSPECIFIED;
    m_colorAspects->transfer = C2Color::TRANSFER_UNSPECIFIED;
    m_colorAspects->matrix = C2Color::MATRIX_UNSPECIFIED;

    m_codedColorAspects = std::make_shared<C2StreamColorAspectsInfo::output>();
    m_codedColorAspects->range = C2Color::RANGE_UNSPECIFIED;
    m_codedColorAspects->transfer = C2Color::TRANSFER_UNSPECIFIED;
    m_codedColorAspects->matrix = C2Color::MATRIX_UNSPECIFIED;
    m_codedColorAspects->primaries = C2Color::PRIMARIES_UNSPECIFIED;

    MFX_ZERO_MEMORY(m_signalInfo);
    MFX_ZERO_MEMORY(m_mfxInputInfo);
    //m_paramStorage.DumpParams();
}

MfxC2EncoderComponent::~MfxC2EncoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    Release();
}

void MfxC2EncoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("c2.intel.av1.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_AV1>);
    registry.RegisterMfxC2Component("c2.intel.avc.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H264>);
    registry.RegisterMfxC2Component("c2.intel.hevc.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H265>);
    registry.RegisterMfxC2Component("c2.intel.vp9.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_VP9>);
}

void MfxC2EncoderComponent::getMaxMinResolutionSupported(
        uint32_t *min_w, uint32_t *min_h, uint32_t *max_w, uint32_t *max_h)
{
    MFX_DEBUG_TRACE_FUNC;

    switch(m_encoderType) {
        case ENCODER_AV1: {
            *min_w = 176;
            *min_h = 144;
            *max_w = 4096;
            *max_h = 4096;
            break;
        }
        case ENCODER_H264: {
            *min_w = 176;
            *min_h = 144;
            *max_w = 4096;
            *max_h = 4096;
            break;
        }
        case ENCODER_H265: {
            *min_w = 176;
            *min_h = 144;
            *max_w = 8192;
            *max_h = 8192;
            break;
        }
        case ENCODER_VP9: {
            *min_w = 128;
            *min_h = 96;
            *max_w = 8192;
            *max_h = 8192;
            break;
        }
    }
}

c2_status_t MfxC2EncoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MfxDev::Create(MfxDev::Usage::Encoder, &m_device);

    if(mfx_res == MFX_ERR_NONE) mfx_res = ResetSettings(); // requires device_ initialized

    if(mfx_res == MFX_ERR_NONE) mfx_res = InitSession();

    return MfxStatusToC2(mfx_res);
}

c2_status_t MfxC2EncoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    m_uSyncedPointsCount = 0;
    mfxStatus mfx_res = MFX_ERR_NONE;
    m_bHeaderSent = false;

    do {
        bool allocator_required = (m_mfxVideoParamsConfig.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY);

        if (allocator_required != m_bAllocatorSet) {

#ifdef USE_ONEVPL
            mfx_res = MFXClose(m_mfxSession);
#else
            mfx_res = m_mfxSession.Close();
#endif
            if (MFX_ERR_NONE != mfx_res) break;

            mfx_res = InitSession();
            if (MFX_ERR_NONE != mfx_res) break;

            // set frame allocator
            if (allocator_required) {

                std::shared_ptr<MfxFrameAllocator> allocator = m_device->GetFrameAllocator();
                if (!allocator) {
                    mfx_res = MFX_ERR_NOT_INITIALIZED;
                    break;
                }

#ifdef USE_ONEVPL
                mfx_res = MFXVideoCORE_SetFrameAllocator(m_mfxSession, &allocator->GetMfxAllocator());
#else
                mfx_res = m_mfxSession.SetFrameAllocator(&allocator->GetMfxAllocator());
#endif
                if (MFX_ERR_NONE != mfx_res) break;

            } else {
#ifdef USE_ONEVPL
                mfx_res = MFXVideoCORE_SetFrameAllocator(m_mfxSession, nullptr);
#else
                mfx_res = m_mfxSession.SetFrameAllocator(nullptr);
#endif
                if (MFX_ERR_NONE != mfx_res) break;
            }
            m_bAllocatorSet = allocator_required;
        }
        m_workingQueue.Start();
        m_waitingQueue.Start();

        if (m_createConfig.dump_output) {

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::ostringstream oss;
            std::tm local_tm;
            localtime_r(&now_c, &local_tm);

            oss << m_name << "-" << std::put_time(std::localtime(&now_c), "%Y%m%d%H%M%S") << ".bin";

            MFX_DEBUG_TRACE_STREAM("Encoder output dump is started to " <<
                MFX_C2_DUMP_DIR << "/" << MFX_C2_DUMP_OUTPUT_SUB_DIR << "/" <<
                oss.str());

            m_outputWriter = std::make_unique<BinaryWriter>(MFX_C2_DUMP_DIR,
                std::vector<std::string>({MFX_C2_DUMP_OUTPUT_SUB_DIR}), oss.str());
        }

    } while(false);

    return C2_OK;
}

c2_status_t MfxC2EncoderComponent::DoStop(bool abort)
{
    MFX_DEBUG_TRACE_FUNC;

    // Working queue should stop first otherwise race condition
    // is possible when waiting queue is stopped (first), but working
    // queue is pushing tasks into it (via EncodeFrameAsync). As a
    // result, such tasks will be processed after next start
    // which is bad as sync point becomes invalid after
    // encoder Close/Init.
    if (abort) {
        m_workingQueue.Abort();
        m_waitingQueue.Abort();
    } else {
        m_workingQueue.Stop();
        m_waitingQueue.Stop();
    }

    while (!m_pendingWorks.empty()) {
        // Other statuses cause libstagefright_ccodec fatal error
        NotifyWorkDone(std::move(m_pendingWorks.front()), C2_NOT_FOUND);
        m_pendingWorks.pop();
    }

    FreeEncoder();

    m_outputWriter.reset();

    return C2_OK;
}

c2_status_t MfxC2EncoderComponent::Release()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    mfxStatus sts = MFX_ERR_NONE;

    FreeSurfacePool();
    m_lockedFrames.clear();
    m_vpp.Close();

#ifdef USE_ONEVPL
    if (m_mfxSession) {
        sts = MFXClose(m_mfxSession);
        m_mfxSession = nullptr;
    }
#else
    sts = m_mfxSession.Close();
#endif

    if (MFX_ERR_NONE != sts) res = MfxStatusToC2(sts);

    if (m_device) {
        m_device->Close();
        if (MFX_ERR_NONE != sts) res = MfxStatusToC2(sts);

        m_device = nullptr;
    }

#ifdef USE_ONEVPL
    if (m_mfxLoader) {
        MFXUnload(m_mfxLoader);
        m_mfxLoader = nullptr;
    }
#endif

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

#ifdef USE_ONEVPL
mfxStatus MfxC2EncoderComponent::InitSession()
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
        MFX_LOG_INFO("%s. ApiVersion: %d.%d. Implementation type: %s. AccelerationMode via: %d",
                 __func__, idesc->ApiVersion.Major, idesc->ApiVersion.Minor,
                (idesc->Impl == MFX_IMPL_TYPE_SOFTWARE) ? "SW" : "HW",
                idesc->AccelerationMode);

        mfx_res = MFXCreateSession(m_mfxLoader, idx, &m_mfxSession);

        MFXDispReleaseImplDescription(m_mfxLoader, idesc);

        if (MFX_ERR_NONE == mfx_res)
            break;

        idx++;
    }

    if (MFX_ERR_NONE != mfx_res) {
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
mfxStatus MfxC2EncoderComponent::InitSession()
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

mfxStatus MfxC2EncoderComponent::ResetSettings()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    MFX_ZERO_MEMORY(m_signalInfo);
    m_signalInfo.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
    m_signalInfo.Header.BufferSz = sizeof(mfxExtVideoSignalInfo);
    m_signalInfo.VideoFormat = 5;
    m_signalInfo.VideoFullRange = 0;
    m_signalInfo.ColourDescriptionPresent = 0;
    m_signalInfo.MatrixCoefficients = 2; // unspecified
    m_signalInfo.ColourPrimaries = 2; // unspecified
    m_signalInfo.TransferCharacteristics = 2; // unspecified

    switch (m_encoderType)
    {
    case ENCODER_AV1:
        m_mfxVideoParamsConfig.mfx.CodecId = MFX_CODEC_AV1;
        break;
    case ENCODER_H264:
        m_mfxVideoParamsConfig.mfx.CodecId = MFX_CODEC_AVC;
        break;
    case ENCODER_H265:
        m_mfxVideoParamsConfig.mfx.CodecId = MFX_CODEC_HEVC;
        break;
    case ENCODER_VP9:
        m_mfxVideoParamsConfig.mfx.CodecId = MFX_CODEC_VP9;
        break;
    default:
        MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
        break;
    }

    mfx_res = mfx_set_defaults_mfxVideoParam_enc(&m_mfxVideoParamsConfig);

    if (m_device) {
        // default pattern: video memory if allocator available
        m_mfxVideoParamsConfig.IOPattern = m_device->GetFrameAllocator() ?
            MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    } else {
        mfx_res = MFX_ERR_NULL_PTR;
    }

    // reset ExtParam
    m_mfxVideoParamsConfig.NumExtParam = 0;
    m_mfxVideoParamsConfig.ExtParam = nullptr;

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

void MfxC2EncoderComponent::AttachExtBuffer()
{
    MFX_DEBUG_TRACE_FUNC;

    if (m_encoderType == ENCODER_H265) {
        mfxExtCodingOption3* codingOption3 = m_mfxVideoParamsConfig.AddExtBuffer<mfxExtCodingOption3>();
        codingOption3->GPB = MFX_CODINGOPTION_OFF;
    }

    if (m_encoderType == ENCODER_H264 || m_encoderType == ENCODER_H265 || m_encoderType == ENCODER_AV1) {
        mfxExtCodingOption* codingOption = m_mfxVideoParamsConfig.AddExtBuffer<mfxExtCodingOption>();
        codingOption->NalHrdConformance = MFX_CODINGOPTION_OFF;

        mfxExtVideoSignalInfo *vsi = m_mfxVideoParamsConfig.AddExtBuffer<mfxExtVideoSignalInfo>();
        memcpy(vsi, &m_signalInfo, sizeof(mfxExtVideoSignalInfo));

        MFX_DEBUG_TRACE_U32(vsi->VideoFormat);
        MFX_DEBUG_TRACE_U32(vsi->VideoFullRange);
        MFX_DEBUG_TRACE_U32(vsi->ColourPrimaries);
        MFX_DEBUG_TRACE_U32(vsi->TransferCharacteristics);
        MFX_DEBUG_TRACE_U32(vsi->MatrixCoefficients);
        MFX_DEBUG_TRACE_U32(vsi->ColourDescriptionPresent);
    } else if (m_encoderType == ENCODER_VP9) {
        mfxExtVP9Param* vp9param = m_mfxVideoParamsConfig.AddExtBuffer<mfxExtVP9Param>();
        vp9param->WriteIVFHeaders = MFX_CODINGOPTION_OFF;
    }
}


mfxStatus MfxC2EncoderComponent::InitEncoder()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    std::lock_guard<std::mutex> lock(m_initEncoderMutex);

    if (MFX_ERR_NONE == mfx_res) {
#ifdef USE_ONEVPL
        m_mfxEncoder.reset(MFX_NEW_NO_THROW(MFXVideoENCODE(m_mfxSession)));
#else
        m_mfxEncoder.reset(MFX_NEW_NO_THROW(MFXVideoENCODE(m_mfxSession)));
#endif
        if (nullptr == m_mfxEncoder) {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
        }

        if (MFX_ERR_NONE == mfx_res) {

            AttachExtBuffer();

            // The BufferSizeInKB is estimated in oneVPL if set to 0 when calling encoder Init().
            // However, the estimated value is not sufficient in some corner cases, such as HEVC with a bitrate of 4 and a resolution of 176x144.
            // In these cases, set BufferSizeInKB manually to a limit to ensure there is enough space to write the bitstream.
            if (m_mfxVideoParamsConfig.mfx.CodecId == MFX_CODEC_HEVC && m_mfxVideoParamsConfig.mfx.TargetKbps < 8) {
                // minBufferSizeInKB is calculated the same method with onevpl
                if (m_mfxVideoParamsConfig.mfx.FrameInfo.Width != 0 &&
                    m_mfxVideoParamsConfig.mfx.FrameInfo.Height != 0 &&
                    m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtN != 0 &&
                    m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtD != 0) {
                    mfxF64 rawDataBitrate = 12.0 * m_mfxVideoParamsConfig.mfx.FrameInfo.Width * m_mfxVideoParamsConfig.mfx.FrameInfo.Height *
                                            m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtN / m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtD;
                    mfxU32 minBufferSizeInKB = mfxU32(std::min<mfxF64>(0xffffffff, rawDataBitrate / 8 / 1000.0 / 1400.0));
                    if (minBufferSizeInKB < 2) {
                        m_mfxVideoParamsConfig.mfx.BufferSizeInKB = 2;
                    }
                }
            }

            MFX_DEBUG_TRACE_MSG("Encoder initializing...");
            MFX_DEBUG_TRACE__mfxVideoParam_enc(m_mfxVideoParamsConfig);

            mfx_res = m_mfxEncoder->Init(&m_mfxVideoParamsConfig);

            MFX_DEBUG_TRACE_MSG("Encoder initialized");
            MFX_DEBUG_TRACE__mfxStatus(mfx_res);

            // ignore warnings
            if (MFX_WRN_PARTIAL_ACCELERATION == mfx_res) {
                MFX_DEBUG_TRACE_MSG("InitEncoder returns MFX_WRN_PARTIAL_ACCELERATION");
                mfx_res = MFX_ERR_NONE;
            }

            if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfx_res) {
                MFX_DEBUG_TRACE_MSG("InitEncoder returns MFX_WRN_INCOMPATIBLE_VIDEO_PARAM");
                mfx_res = MFX_ERR_NONE;
            }

            if (MFX_ERR_NONE == mfx_res) {
                // Query required surfaces number for encoder
                mfxFrameAllocRequest encRequest = {};
                mfx_res = m_mfxEncoder->QueryIOSurf(&m_mfxVideoParamsConfig, &encRequest);
                if (MFX_ERR_NONE == mfx_res) {
                   if (m_encSrfNum < encRequest.NumFrameSuggested) {
                        ALOGE("More buffer needed for encoder input! Actual: %d. Expected: %d",
                        m_encSrfNum, encRequest.NumFrameSuggested);
                        mfx_res = MFX_ERR_MORE_SURFACE;
                   }
                } else {
                    MFX_DEBUG_TRACE_MSG("QueryIOSurf failed");
                    mfx_res = MFX_ERR_UNKNOWN;
                }
            }
        }

        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = m_mfxEncoder->GetVideoParam(&m_mfxVideoParamsState);
            MFX_DEBUG_TRACE__mfxVideoParam_enc(m_mfxVideoParamsState);
        }

        if (MFX_ERR_NONE != mfx_res) {
            FreeEncoder();
        }
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2EncoderComponent::InitVPP(C2FrameData& buf_pack)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;
    c2_status_t res;
    const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

    std::unique_ptr<C2ConstGraphicBlock> c_graph_block;
    std::unique_ptr<const C2GraphicView> c2_graphic_view_;

    res = GetC2ConstGraphicBlock(buf_pack, &c_graph_block);
    if(C2_OK != res) return MFX_ERR_NONE;

    m_mfxInputInfo = m_mfxVideoParamsConfig.mfx.FrameInfo;
    m_mfxInputInfo.Width = c_graph_block->width();
    m_mfxInputInfo.Height = c_graph_block->height();
    MFX_DEBUG_TRACE_I32(c_graph_block->width());
    MFX_DEBUG_TRACE_I32(c_graph_block->height());

    res = MapConstGraphicBlock(*c_graph_block, TIMEOUT_NS, &c2_graphic_view_);

    switch (c2_graphic_view_->layout().type) {
    case C2PlanarLayout::TYPE_RGB: {
        // need color convert to YUV
        MfxC2VppWrappParam param;
        mfxFrameInfo frame_info;

#ifdef USE_ONEVPL
        param.session = m_mfxSession;
#else
        param.session = &m_mfxSession;
#endif
        frame_info = m_mfxVideoParamsConfig.mfx.FrameInfo;
        param.frame_info = &frame_info;
        param.frame_info->FourCC = MFX_FOURCC_RGB4;
        param.allocator = m_device->GetFrameAllocator();
        param.conversion = ARGB_TO_NV12;

        mfx_res = m_vpp.Init(&param);
        m_inputVppType = param.conversion;
    }
        break;
    case C2PlanarLayout::TYPE_YUV: {
        uint32_t width, height, format, stride, igbp_slot, generation;
        uint64_t usage, igbp_id;
        android::_UnwrapNativeCodec2GrallocMetadata(c_graph_block->handle(), &width, &height, &format, &usage,
                                                &stride, &generation, &igbp_id, &igbp_slot);
        if (format == HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL) {
            m_mfxVideoParamsConfig.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
        } else if (format == HAL_PIXEL_FORMAT_P010_INTEL) {
            m_mfxVideoParamsConfig.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;
            m_mfxVideoParamsConfig.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
            m_mfxVideoParamsConfig.mfx.FrameInfo.BitDepthLuma = 10;
            m_mfxVideoParamsConfig.mfx.FrameInfo.BitDepthChroma = 10;
            m_mfxVideoParamsConfig.mfx.FrameInfo.Shift = 1;
        } else {
            if ((!igbp_id && !igbp_slot) || (!igbp_id && igbp_slot == 0xffffffff)) {
                //No surface & BQ
                m_mfxVideoParamsConfig.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
#ifdef USE_ONEVPL
                mfx_res = MFXVideoCORE_SetFrameAllocator(m_mfxSession, nullptr);
#else
                mfx_res = m_mfxSession.SetFrameAllocator(nullptr);
#endif
                m_bAllocatorSet = false;
                MFX_LOG_INFO("Format = 0x%x. System memory is being used for encoding!", format);
                if (MFX_ERR_NONE != mfx_res) MFX_DEBUG_TRACE_MSG("SetFrameAllocator failed");
            }

            if (format == HAL_PIXEL_FORMAT_YCBCR_P010) {
                m_mfxVideoParamsConfig.mfx.FrameInfo.FourCC = MFX_FOURCC_P010;
                m_mfxVideoParamsConfig.mfx.FrameInfo.BitDepthLuma = 10;
                m_mfxVideoParamsConfig.mfx.FrameInfo.BitDepthChroma = 10;
                m_mfxVideoParamsConfig.mfx.FrameInfo.Shift = 1;
            }

            mfx_res = AllocateSurfacePool();
            if (MFX_ERR_NONE != mfx_res) MFX_DEBUG_TRACE_MSG("AllocateSurfacePool failed");

        }

        m_inputVppType = CONVERT_NONE;
    }
        break;
    default:
        MFX_DEBUG_TRACE_PRINTF("Unsupported layout: 0x%x", (uint32_t)c2_graphic_view_->layout().type);
        break;
    }

    if (MFX_ERR_NONE == mfx_res) m_bVppDetermined = true;

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

void MfxC2EncoderComponent::FreeEncoder()
{
    MFX_DEBUG_TRACE_FUNC;

    if(nullptr != m_mfxEncoder) {
        m_mfxEncoder->Close();
        m_mfxEncoder = nullptr;
    }

    // reset ExtParam
    m_mfxVideoParamsConfig.NumExtParam = 0;
    m_mfxVideoParamsConfig.ExtParam = nullptr;
}

void MfxC2EncoderComponent::RetainLockedFrame(MfxC2FrameIn&& input)
{
    MFX_DEBUG_TRACE_FUNC;

    if(input.GetMfxFrameSurface()->Data.Locked) {
        m_lockedFrames.emplace_back(std::move(input));
    }
}

mfxStatus MfxC2EncoderComponent::EncodeFrameAsync(
    mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs,
    mfxSyncPoint *syncp)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus sts = MFX_ERR_NONE;

    int trying_count = 0;
    const int MAX_TRYING_COUNT = 200;
    const auto timeout = std::chrono::milliseconds(5);

    do {
      sts = m_mfxEncoder->EncodeFrameAsync(ctrl, surface, bs, syncp);
      ++trying_count;

      if (MFX_WRN_DEVICE_BUSY == sts) {

        if (trying_count >= MAX_TRYING_COUNT) {
            MFX_DEBUG_TRACE_MSG("Too many MFX_WRN_DEVICE_BUSY from EncodeFrameAsync");
            sts = MFX_ERR_DEVICE_FAILED;
            break;
        }

        MFX_DEBUG_TRACE_STREAM("received MFX_WRN_DEVICE_BUSY [trying_count = " << trying_count << ", m_uSyncedPointsCount = " <<
            m_uSyncedPointsCount.load() << "]");
        std::unique_lock<std::mutex> lock(m_devBusyMutex);
        m_devBusyCond.wait_for(lock, timeout, [this] {
            if (m_uSyncedPointsCount < m_mfxVideoParamsState.AsyncDepth) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return true;
            }
            else {
                return false;
            }});
      }
    } while (MFX_WRN_DEVICE_BUSY == sts);

    MFX_DEBUG_TRACE__mfxStatus(sts);
    return sts;
}

mfxStatus MfxC2EncoderComponent::AllocateSurfacePool()
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus sts = MFX_ERR_NONE;

    const mfxFrameInfo frame_info = m_mfxVideoParamsConfig.mfx.FrameInfo;

    // External (application) allocation of encoder surfaces
    m_encSrfPool =
        (mfxFrameSurface1 *)calloc(sizeof(mfxFrameSurface1), m_encSrfNum);
    if (!m_encSrfPool) {
        return MFX_ERR_MEMORY_ALLOC;
    }

    for (uint32_t i = 0; i < m_encSrfNum; i++) {
         MFX_ZERO_MEMORY(m_encSrfPool[i]);
    }

    if (MFX_IOPATTERN_IN_SYSTEM_MEMORY == m_mfxVideoParamsConfig.IOPattern) {
        if (MFX_C2_IS_COPY_NEEDED(MFX_MEMTYPE_SYSTEM_MEMORY, m_mfxInputInfo, frame_info)) {
            sts = MFXAllocSystemMemorySurfacePool(&m_encOutBuf, m_encSrfPool, frame_info, m_encSrfNum);
        }
    }

    return sts;
}

void MfxC2EncoderComponent::FreeSurfacePool()
{
    MFX_DEBUG_TRACE_FUNC;

    MFXFreeSystemMemorySurfacePool(m_encOutBuf, m_encSrfPool);
    m_encOutBuf = nullptr;
    m_encSrfPool= nullptr;
    m_encSrfNum = 0;
}

c2_status_t MfxC2EncoderComponent::AllocateBitstream(const std::unique_ptr<C2Work>& work,
    MfxC2BitstreamOut* mfx_bitstream)
{
    // TODO: allocation pool is required here
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        MFX_DEBUG_TRACE_I32(m_mfxVideoParamsState.mfx.BufferSizeInKB);
        MFX_DEBUG_TRACE_I32(m_mfxVideoParamsState.mfx.BRCParamMultiplier);
        mfxU32 required_size = m_mfxVideoParamsState.mfx.BufferSizeInKB * 1000 * m_mfxVideoParamsState.mfx.BRCParamMultiplier;
        MFX_DEBUG_TRACE_I32(required_size);

        if(work->worklets.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple worklets");
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

        if(worklet->output.buffers.size() != 0) {
            MFX_DEBUG_TRACE_MSG("Caller is not supposed to allocate output");
            res = C2_BAD_VALUE;
            break;
        }

        C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        std::shared_ptr<C2LinearBlock> out_block;

        res = m_c2Allocator->fetchLinearBlock(required_size, mem_usage, &out_block);
        if(C2_OK != res) break;

        res = MfxC2BitstreamOut::Create(std::move(out_block), TIMEOUT_NS, mfx_bitstream);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2EncoderComponent::ApplyWorkTunings(C2Work& work)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {

        if (work.worklets.size() != 1) {
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2Worklet>& worklet = work.worklets.front();
        if (nullptr == worklet) {
            res = C2_BAD_VALUE;
            break;
        }

        if (worklet->tunings.size() != 0) {
            // need this temp vector as cannot init vector<smth const> in one step
            std::vector<C2Param*> temp;
            std::transform(worklet->tunings.begin(), worklet->tunings.end(), std::back_inserter(temp),
                [] (const std::unique_ptr<C2Tuning>& p) { return p.get(); } );

            std::vector<C2Param*> params(temp.begin(), temp.end());

            std::vector<std::unique_ptr<C2SettingResult>> failures;
            {
                // These parameters update comes with C2Work from work queue,
                // there is no guarantee that state is not changed meanwhile
                // in contrast to Config method protected with state mutex.
                // So AcquireStableStateLock is needed here.
                std::unique_lock<std::mutex> lock = AcquireStableStateLock(true);
                res = config(params, C2_DONT_BLOCK, &failures);
                DoUpdateMfxParam(params, &failures, false);
            }
            for(auto& failure : failures) {
                worklet->failures.push_back(std::move(failure));
            }
        }
    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

void MfxC2EncoderComponent::DoWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    c2_status_t res = C2_OK;

    do {
        if (!m_c2Allocator) {
            res = GetCodec2BlockPool(C2BlockPool::BASIC_LINEAR,
                shared_from_this(), &m_c2Allocator);
            if (res != C2_OK) break;
        }

        C2FrameData& input = work->input;
        MfxC2FrameIn mfx_frame_in;

        if (!m_bVppDetermined) {
            mfxStatus mfx_sts = InitVPP(input);
            if(MFX_ERR_NONE != mfx_sts) {
                MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                res = MfxStatusToC2(mfx_sts);
                break;
            }
        }

        std::shared_ptr<MfxFrameConverter> frame_converter;
        if (m_mfxVideoParamsConfig.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY) {
            frame_converter = m_device->GetFrameConverter();
        }

        if (frame_converter) {
            std::unique_ptr<mfxFrameSurface1> unique_mfx_frame =
                    std::make_unique<mfxFrameSurface1>();
            mfxFrameSurface1* pSurfaceToEncode = nullptr;
            std::unique_ptr<const C2GraphicView> c_graph_view;

            std::unique_ptr<C2ConstGraphicBlock> c_graph_block;
            res = GetC2ConstGraphicBlock(input, &c_graph_block);
            MapConstGraphicBlock(*c_graph_block, TIMEOUT_NS, &c_graph_view);

            mfxMemId mem_id = nullptr;
            bool decode_target = false;
            native_handle_t *grallocHandle = android::UnwrapNativeCodec2GrallocHandle(c_graph_block->handle());
            // From Android U, the get function of IMapper4 will check whether the buffer handle is reserved.
            // So we need to call importBuffer before getting the buffer's info.
#if PLATFORM_SDK_VERSION >= 34 && defined(USE_GRALLOC4) // Android 14(U)
            buffer_handle_t importedHandle = MfxGrallocInstance::getInstance()->ImportBuffer(grallocHandle);

            mfxStatus mfx_sts = frame_converter->ConvertGrallocToVa(importedHandle,
                                         decode_target, &mem_id);

            native_handle_delete(const_cast<native_handle_t *>(importedHandle));
            importedHandle = nullptr;
#else
            mfxStatus mfx_sts = frame_converter->ConvertGrallocToVa(grallocHandle,
                                         decode_target, &mem_id);

            native_handle_delete(grallocHandle);
            grallocHandle = nullptr;
#endif

            if (MFX_ERR_NONE != mfx_sts) {
                res = MfxStatusToC2(mfx_sts);
                MFX_ZERO_MEMORY(mfx_frame_in);
                break;
            }

            if (m_inputVppType != CONVERT_NONE) {
                InitMfxFrameHW(input.ordinal.timestamp.peeku(), input.ordinal.frameIndex.peeku(),
                    mem_id, c_graph_block->width(), c_graph_block->height(), MFX_FOURCC_RGB4,
                    m_mfxVideoParamsConfig.mfx.FrameInfo, unique_mfx_frame.get());

                m_vpp.ProcessFrameVpp(unique_mfx_frame.get(), &pSurfaceToEncode);
            } else {
                pSurfaceToEncode = new mfxFrameSurface1;
                if (!pSurfaceToEncode) {
                    res = C2_NO_MEMORY;
                    break;
                }
                InitMfxFrameHW(input.ordinal.timestamp.peeku(), input.ordinal.frameIndex.peeku(),
                    mem_id, c_graph_block->width(), c_graph_block->height(), m_mfxVideoParamsConfig.mfx.FrameInfo.FourCC,
                    m_mfxVideoParamsConfig.mfx.FrameInfo, pSurfaceToEncode);
            }
            res = mfx_frame_in.init(NULL, std::move(c_graph_view), input, pSurfaceToEncode);
        } else {
            uint32_t nIndex = MFXGetFreeSurfaceIdx(m_encSrfPool, m_encSrfNum);
            MFX_DEBUG_TRACE_I32(nIndex);
            if (nIndex >= m_encSrfNum) {
                mfxStatus mfx_sts = MFX_ERR_NOT_FOUND;
                MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                res = MfxStatusToC2(mfx_sts);
                break;
            }
            res = mfx_frame_in.init(std::move(frame_converter), input, m_mfxVideoParamsConfig.mfx.FrameInfo, &m_encSrfPool[nIndex], TIMEOUT_NS);
        }
        if(C2_OK != res) break;

        if(nullptr == m_mfxEncoder) {
            mfxStatus mfx_sts = InitEncoder();
            if(MFX_ERR_NONE != mfx_sts) {
                MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                res = MfxStatusToC2(mfx_sts);
                break;
            }
        }

        MfxC2BitstreamOut mfx_bitstream;
        res = AllocateBitstream(work, &mfx_bitstream);
        if(C2_OK != res) break;

        mfxSyncPoint sync_point;

        res = ApplyWorkTunings(*work);
        if(C2_OK != res) break;

        std::unique_ptr<mfxEncodeCtrl> encode_ctrl = m_encoderControl.AcquireEncodeCtrl();

        mfxStatus mfx_sts = EncodeFrameAsync(encode_ctrl.get(),
            mfx_frame_in.GetMfxFrameSurface(), mfx_bitstream.GetMfxBitstream(), &sync_point);

        if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfx_sts) mfx_sts = MFX_ERR_NONE;

        if( (MFX_ERR_NONE != mfx_sts) && (MFX_ERR_MORE_DATA != mfx_sts) ) {
            MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
            res = MfxStatusToC2(mfx_sts);
            break;
        }

        // XXX: "Big parameter passed by value(PASS_BY_VALUE)" by Coverity scanning.
        // mfx_frame of type size 136 bytes.
        m_waitingQueue.Push( [ mfx_frame = std::move(mfx_frame_in), this ] () mutable {
            RetainLockedFrame(std::move(mfx_frame));
        } );

        m_pendingWorks.push(std::move(work));

        if(MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<C2Work> work = std::move(m_pendingWorks.front());

            m_pendingWorks.pop();

            m_waitingQueue.Push(
                [ work = std::move(work), ec = std::move(encode_ctrl), bs = std::move(mfx_bitstream), sync_point, this ] () mutable {
                WaitWork(std::move(work), std::move(ec), std::move(bs), sync_point);
            } );

            {
                std::unique_lock<std::mutex> lock(m_devBusyMutex);
                ++m_uSyncedPointsCount;
            }
        }

        if(MFX_ERR_MORE_DATA == mfx_sts) mfx_sts = MFX_ERR_NONE;

    } while(false); // fake loop to have a cleanup point there

    if(C2_OK != res) { // notify listener in case of failure only
        ReturnEmptyWork(std::move(work), res);
    }
}

void MfxC2EncoderComponent::Drain(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    while (!m_pendingWorks.empty()) {

        MfxC2BitstreamOut mfx_bitstream;
        res = AllocateBitstream(m_pendingWorks.front(), &mfx_bitstream);
        if(C2_OK != res) break;

        mfxSyncPoint sync_point;

        std::unique_ptr<mfxEncodeCtrl> encode_ctrl = m_encoderControl.AcquireEncodeCtrl();

        mfxStatus mfx_sts = EncodeFrameAsync(encode_ctrl.get(),
            nullptr/*input surface*/, mfx_bitstream.GetMfxBitstream(), &sync_point);

        if (MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<C2Work> work = std::move(m_pendingWorks.front());

            m_pendingWorks.pop();

            m_waitingQueue.Push(
                [ work = std::move(work), ec = std::move(encode_ctrl), bs = std::move(mfx_bitstream), sync_point, this ] () mutable {
                WaitWork(std::move(work), std::move(ec), std::move(bs), sync_point);
            } );

            {
                std::unique_lock<std::mutex> lock(m_devBusyMutex);
                ++m_uSyncedPointsCount;
            }
        } else {
            // MFX_ERR_MORE_DATA is an error here too -
            // we are calling EncodeFrameAsync times exactly how many outputs should be fetched
            res = MfxStatusToC2(mfx_sts);
            break;
        }
    }

    // eos work, should be sent after last work returned
    if (work) {
        m_waitingQueue.Push([work = std::move(work), this]() mutable {

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

            worklet->output.flags = (C2FrameData::flags_t)(work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
            worklet->output.ordinal = work->input.ordinal;

            NotifyWorkDone(std::move(work), C2_OK);
        });
    }

    if(C2_OK != res) {
        while(!m_pendingWorks.empty()) {
            NotifyWorkDone(std::move(m_pendingWorks.front()), res);
            m_pendingWorks.pop();
        }
    }
}

void MfxC2EncoderComponent::ReturnEmptyWork(std::unique_ptr<C2Work>&& work, c2_status_t res)
{
    MFX_DEBUG_TRACE_FUNC;

    uint32_t flags = 0;
    // Pass end of stream flag only
    if (work->input.flags & C2FrameData::FLAG_END_OF_STREAM) {
        flags |= C2FrameData::FLAG_END_OF_STREAM;
        MFX_DEBUG_TRACE_MSG("signalling eos");
    }

    if (work->worklets.size() > 0) {
        std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
        worklet->output.flags = (C2FrameData::flags_t)flags;
        worklet->output.buffers.clear();
        worklet->output.ordinal = work->input.ordinal;
    }

    NotifyWorkDone(std::move(work), res);
}

void MfxC2EncoderComponent::WaitWork(std::unique_ptr<C2Work>&& work,
    std::unique_ptr<mfxEncodeCtrl>&& encode_ctrl,
    MfxC2BitstreamOut&& bit_stream, mfxSyncPoint sync_point)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

#ifdef USE_ONEVPL
    mfx_res = MFXVideoCORE_SyncOperation(m_mfxSession, sync_point, MFX_TIMEOUT_INFINITE);
#else
    mfx_res = m_mfxSession.SyncOperation(sync_point, MFX_TIMEOUT_INFINITE);
#endif

    if (MFX_ERR_NONE != mfx_res) {
        MFX_DEBUG_TRACE_MSG("SyncOperation failed");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    }

    // checking for unlocked surfaces and releasing them
    m_lockedFrames.remove_if(
        [] (const MfxC2FrameIn& mfx_frame)->bool { return !mfx_frame.GetMfxFrameSurface()->Data.Locked; } );

    // release encode_ctrl
    encode_ctrl = nullptr;

    if(MFX_ERR_NONE == mfx_res) {

        //C2Event event; // not supported yet, left for future use
        //event.fire(); // pre-fire event as output buffer is ready to use

        mfxBitstream* mfx_bitstream = bit_stream.GetMfxBitstream();
        MFX_DEBUG_TRACE_P(mfx_bitstream);

        if(!mfx_bitstream) mfx_res = MFX_ERR_NULL_PTR;
        else {
            MFX_DEBUG_TRACE_STREAM(NAMED(mfx_bitstream->DataOffset) << NAMED(mfx_bitstream->DataLength));

            if (m_outputWriter && mfx_bitstream->DataLength > 0) {
                m_outputWriter->Write(mfx_bitstream->Data + mfx_bitstream->DataOffset,
                    mfx_bitstream->DataLength);
            }

            C2ConstLinearBlock const_linear = bit_stream.GetC2LinearBlock()->share(
                mfx_bitstream->DataOffset,
                mfx_bitstream->DataLength, C2Fence()/*event.fence()*/);
            C2Buffer out_buffer = MakeC2Buffer( { std::move(const_linear) } );
            if ((mfx_bitstream->FrameType & MFX_FRAMETYPE_IDR) != 0 || (mfx_bitstream->FrameType & MFX_FRAMETYPE_I) != 0 ) {
                out_buffer.setInfo(std::make_shared<C2StreamPictureTypeMaskInfo::output>(0u/*stream id*/, C2Config::SYNC_FRAME));
            }

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

            worklet->output.flags = work->input.flags;

            // VPX unsupport SPS PPS
            if (!m_bHeaderSent && (m_encoderType == ENCODER_H264 || m_encoderType == ENCODER_H265)) {

                mfxExtCodingOptionSPSPPS* spspps{};
                mfxExtCodingOptionVPS* vps{};
                MfxVideoParamsWrapper video_param {};

                mfxU8 buf[256/*VPS*/ + 1024/*SPS*/ + 128/*PPS*/] = {0};
                try {
                    spspps = video_param.AddExtBuffer<mfxExtCodingOptionSPSPPS>();

                    if (ENCODER_H265 == m_encoderType)
                        vps = video_param.AddExtBuffer<mfxExtCodingOptionVPS>();

                    spspps->SPSBuffer = buf;
                    spspps->SPSBufSize = 1024;

                    spspps->PPSBuffer = buf + spspps->SPSBufSize;
                    spspps->PPSBufSize = 128;

                    if (ENCODER_H265 == m_encoderType) {
                        vps->VPSBuffer = spspps->PPSBuffer + spspps->PPSBufSize;
                        vps->VPSBufSize = 256;
                    }

                    mfx_res = m_mfxEncoder->GetVideoParam(&video_param);
                } catch(std::exception err) {
                    MFX_DEBUG_TRACE_STREAM("Error:" << err.what());
                    mfx_res = MFX_ERR_MEMORY_ALLOC;
                }

                if (MFX_ERR_NONE == mfx_res) {

                    int header_size = spspps->SPSBufSize + spspps->PPSBufSize;
                    if (ENCODER_H265 == m_encoderType)
                        header_size += vps->VPSBufSize;

                    std::unique_ptr<C2StreamInitDataInfo::output> csd =
                        C2StreamInitDataInfo::output::AllocUnique(header_size, 0u);

                    if (ENCODER_H265 == m_encoderType)
                        MFX_DEBUG_TRACE_STREAM("VPS: " << FormatHex(vps->VPSBuffer, vps->VPSBufSize));

                    MFX_DEBUG_TRACE_STREAM("SPS: " << FormatHex(spspps->SPSBuffer, spspps->SPSBufSize));
                    MFX_DEBUG_TRACE_STREAM("PPS: " << FormatHex(spspps->PPSBuffer, spspps->PPSBufSize));

                    uint8_t* dst = csd->m.value;

                    // Copy buffers in the order of VPS, SPS, PPS for HEVC or SPS, PPS for AVC
                    if (ENCODER_H265 == m_encoderType) {
                        std::copy(vps->VPSBuffer, vps->VPSBuffer + vps->VPSBufSize, dst);
                        dst += vps->VPSBufSize;
                    }

                    std::copy(spspps->SPSBuffer, spspps->SPSBuffer + spspps->SPSBufSize, dst);
                    dst += spspps->SPSBufSize;

                    std::copy(spspps->PPSBuffer, spspps->PPSBuffer + spspps->PPSBufSize, dst);

                    work->worklets.front()->output.configUpdate.push_back(std::move(csd));

                    worklet->output.flags = (C2FrameData::flags_t)(worklet->output.flags |
                        C2FrameData::FLAG_CODEC_CONFIG);

                    m_bHeaderSent = true;
                }

                if (MFX_ERR_NONE == mfx_res) {
                    // validate coded color aspects here
                    std::shared_ptr<C2StreamColorAspectsInfo::output> codedColorAspects = getCodedColorAspects_l();
                    if (CodedColorAspectsDiffer(std::move(codedColorAspects))) {
                        // not a fatal error
                        MFX_LOG_INFO("Color aspects are not coded as expected!");
                    }
                }
            }

            worklet->output.ordinal = work->input.ordinal;

            worklet->output.buffers.push_back(std::make_shared<C2Buffer>(out_buffer));
        }
    }

    // By resetting bit_stream we dispose of the bitrstream mapping here.
    bit_stream = MfxC2BitstreamOut();
    NotifyWorkDone(std::move(work), MfxStatusToC2(mfx_res));

    {
      std::unique_lock<std::mutex> lock(m_devBusyMutex);
      --m_uSyncedPointsCount;
    }
    m_devBusyCond.notify_one();
}

std::unique_ptr<mfxVideoParam> MfxC2EncoderComponent::GetParamsView() const
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<mfxVideoParam> res = std::make_unique<mfxVideoParam>();
    mfxStatus sts = MFX_ERR_NONE;

    if(nullptr == m_mfxEncoder) {
        MfxVideoParamsWrapper* in_params = const_cast<MfxVideoParamsWrapper*>(&m_mfxVideoParamsConfig);

        res->mfx.CodecId = in_params->mfx.CodecId;

        MFX_DEBUG_TRACE__mfxVideoParam_enc((*in_params));
        sts = MFXVideoENCODE_Query(
#ifdef USE_ONEVPL
            m_mfxSession,
#else
            (mfxSession)*const_cast<MFXVideoSession*>(&m_mfxSession),
#endif
            in_params, res.get());
        MFX_DEBUG_TRACE__mfxVideoParam_enc((*res));
    } else {
        sts = m_mfxEncoder->GetVideoParam(res.get());
    }

    MFX_DEBUG_TRACE__mfxStatus(sts);
    if (MFX_ERR_NONE != sts) {
        res = nullptr;
    } else {
        MFX_DEBUG_TRACE__mfxVideoParam_enc((*res));
    }
    return res;
}

c2_status_t MfxC2EncoderComponent::UpdateC2Param(C2Param::Index index) const
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    switch (index.typeIndex()) {
        case kParamIndexFrameRate: {
            m_frameRate->value = (float)m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtN / 
                            m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtD;
            break;
        }
        case kParamIndexBitrate: {
            m_bitrate->value = m_mfxVideoParamsConfig.mfx.TargetKbps * 1000; // Convert from Kbps to bps
            break;
        }
        case kParamIndexProfileLevel: {
            switch (m_encoderType) {
                case ENCODER_AV1:
                    Av1ProfileMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecProfile, &m_profileLevel->profile);
                    Av1LevelMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecLevel, &m_profileLevel->level);
                    break;
                case ENCODER_H264:
                    AvcProfileMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecProfile, &m_profileLevel->profile);
                    AvcLevelMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecLevel, &m_profileLevel->level);
                    break;
                case ENCODER_H265:
                    HevcProfileMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecProfile, &m_profileLevel->profile);
                    HevcLevelMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecLevel, &m_profileLevel->level);
                    break;
                case ENCODER_VP9:
                    Vp9ProfileMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecProfile, &m_profileLevel->profile);
                    break;
                default:
                    MFX_DEBUG_TRACE_STREAM("cannot find the type " << m_encoderType );
                    break;
            }
            MFX_DEBUG_TRACE_STREAM("m_profileLevel->profile = " << m_profileLevel->profile << ", m_profileLevel->level = "
                            << m_profileLevel->level << ", mfx.CodecProfile = " << m_mfxVideoParamsConfig.mfx.CodecProfile
                            << ", mfx.CodecLevel = " << m_mfxVideoParamsConfig.mfx.CodecLevel);
            break;
        }
        case kParamIndexSyncFrameInterval: {
            m_syncFramePeriod->value = m_mfxVideoParamsConfig.mfx.GopPicSize;
            break;
        }
        default:
            break;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2EncoderComponent::UpdateMfxParamToC2(
    std::unique_lock<std::mutex> m_statelock,
    const std::vector<C2Param*> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    (void)m_statelock;
    (void)mayBlock;
    (void)heapParams;

    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(m_initEncoderMutex);

    c2_status_t res = C2_OK;

    // 1st cycle on stack params
    for (C2Param* param : stackParams) {
        UpdateC2Param(param->index());
    }
    // 2nd cycle on heap params
    for (C2Param::Index param_index : heapParamIndices) {
        UpdateC2Param(param_index);
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

void MfxC2EncoderComponent::DoUpdateMfxParam(const std::vector<C2Param*> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures,
    bool queue_update)
{
    MFX_DEBUG_TRACE_FUNC;

    for (const C2Param* param : params) {
        // applying parameter
        switch (C2Param::Type(param->type()).typeIndex()) {
            case kParamIndexPictureSize: {
                MFX_DEBUG_TRACE_STREAM("updating m_size->width = " << m_size->width << ", m_size->height = " << m_size->height);
                m_mfxVideoParamsConfig.mfx.FrameInfo.Width = MFX_MEM_ALIGN(m_size->width, 16);
                m_mfxVideoParamsConfig.mfx.FrameInfo.Height = MFX_MEM_ALIGN(m_size->height, 16);
                m_mfxVideoParamsConfig.mfx.FrameInfo.CropX = 0;
                m_mfxVideoParamsConfig.mfx.FrameInfo.CropY = 0;
                m_mfxVideoParamsConfig.mfx.FrameInfo.CropW = m_size->width;
                m_mfxVideoParamsConfig.mfx.FrameInfo.CropH = m_size->height;
                break;
            }
            case kParamIndexFrameRate: {
                float framerate_value = m_frameRate->value;

                if (m_encoderType == ENCODER_H264 && framerate_value > MFX_MAX_H264_FRAMERATE) {
                    framerate_value = MFX_MAX_H264_FRAMERATE;
                }
                else if (m_encoderType == ENCODER_H265 && framerate_value > MFX_MAX_H265_FRAMERATE) {
                    framerate_value = MFX_MAX_H265_FRAMERATE;
                }

                m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtN = uint64_t(framerate_value * 1000); // keep 3 sign after dot
                m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtD = 1000;
                break;
            }
            case kParamIndexBitrate: {
                // MFX_RATECONTROL_CQP parameter is valid only during initialization.
                if (m_mfxVideoParamsConfig.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                    uint32_t bitrate_value = m_bitrate->value;
                    if (m_state == State::STOPPED) {
                        m_mfxVideoParamsConfig.mfx.TargetKbps = bitrate_value / 1000; // Convert from bps to Kbps
                    } else {
                        auto update_bitrate_value = [this, bitrate_value, queue_update, failures, param] () {
                            MFX_DEBUG_TRACE_FUNC;
                            // MDSK strongly recommended to retrieve the actual working parameters by MFXVideoENCODE_GetVideoParam
                            // function before making any changes to bitrate settings.
                            if (nullptr != m_mfxEncoder) {
                                mfxStatus mfx_res = m_mfxEncoder->GetVideoParam(&m_mfxVideoParamsConfig);
                                if (MFX_ERR_NONE != mfx_res) {
                                    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
                                    return;
                                }
                            }
                            m_mfxVideoParamsConfig.mfx.TargetKbps = bitrate_value / 1000; // Convert from bps to Kbps
                            // If application sets NalHrdConformance option in mfxExtCodingOption structure to ON, the only allowed bitrate control mode is VBR.
                            // If OFF, all bitrate control modes are available.In CBR and AVBR modes the application can
                            // change TargetKbps, in VBR mode the application can change TargetKbps and MaxKbps values.
                            // Such change in bitrate will not result in generation of a new key-frame or sequence header.
                            if (m_encoderType == ENCODER_H265 && m_mfxVideoParamsConfig.mfx.RateControlMethod == MFX_RATECONTROL_CBR) {
                                mfxExtEncoderResetOption* resetOption = m_mfxVideoParamsConfig.AddExtBuffer<mfxExtEncoderResetOption>();
                                resetOption->StartNewSequence = MFX_CODINGOPTION_ON;
                            }
                            if (nullptr != m_mfxEncoder) {
                                {   // waiting for encoding completion of all enqueued frames
                                    std::unique_lock<std::mutex> lock(m_devBusyMutex);
                                    // set big enough value to not hang if something unexpected happens
                                    const auto timeout = std::chrono::seconds(1);
                                    bool wait_res = m_devBusyCond.wait_for(lock, timeout, [this] { return m_uSyncedPointsCount == 0; } );
                                    if (!wait_res) {
                                        MFX_DEBUG_TRACE_MSG("WRN: Some encoded frames might skip during tunings change.");
                                    }
                                }
                                mfxStatus reset_sts = m_mfxEncoder->Reset(&m_mfxVideoParamsConfig);
                                MFX_DEBUG_TRACE__mfxStatus(reset_sts);
                                if (MFX_ERR_NONE != reset_sts) {
                                    if (!queue_update) {
                                        //failures->push_back(MakeC2SettingResult(C2ParamField(param),
                                        //    C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                                    }
                                }
                            }
                        };

                        MFX_DEBUG_TRACE_PRINTF("updating bitrate from %d to %d.",
                                m_mfxVideoParamsConfig.mfx.TargetKbps, bitrate_value / 1000);
                        Drain(nullptr);

                        if (queue_update) {
                            m_workingQueue.Push(std::move(update_bitrate_value));
                        } else {
                            update_bitrate_value();
                        }
                    }
                } else {
                    //failures->push_back(MakeC2SettingResult(C2ParamField(param),
                    //    C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                }
                break;
            }
            case kParamIndexBitrateMode: {
                mfxStatus sts = MFX_ERR_NONE;
                switch (m_bitrateMode->value) {
                    case C2Config::BITRATE_CONST:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, &m_mfxVideoParamsConfig);
                        break;
                    case C2Config::BITRATE_VARIABLE:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_VBR, &m_mfxVideoParamsConfig);
                        break;
                    case C2Config::BITRATE_IGNORE:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CQP, &m_mfxVideoParamsConfig);
                        break;
                    default:
                        sts = MFX_ERR_INVALID_VIDEO_PARAM;
                        break;
                }
                MFX_DEBUG_TRACE_STREAM("set m_bitrateMode->value = " << m_bitrateMode->value);
                if(MFX_ERR_NONE != sts) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexIntraRefresh: {
                MFX_DEBUG_TRACE_MSG("got kParamIndexIntraRefresh");
                // TODO:
                break;
            }
            case kParamIndexProfileLevel: {
                switch (m_encoderType) {
                    case ENCODER_AV1:
                        Av1ProfileAndroidToMfx(m_profileLevel->profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        Av1LevelAndroidToMfx(m_profileLevel->level, &m_mfxVideoParamsConfig.mfx.CodecLevel);
                        break;
                    case ENCODER_H264:
                        AvcProfileAndroidToMfx(m_profileLevel->profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        AvcLevelAndroidToMfx(m_profileLevel->level, &m_mfxVideoParamsConfig.mfx.CodecLevel);
                        break;
                    case ENCODER_H265:
                        HevcProfileAndroidToMfx(m_profileLevel->profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        HevcLevelAndroidToMfx(m_profileLevel->level, &m_mfxVideoParamsConfig.mfx.CodecLevel);
                        break;
                    case ENCODER_VP9:
                        Vp9ProfileAndroidToMfx(m_profileLevel->profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        break;
                    default:
                        break;
                }
                MFX_DEBUG_TRACE_STREAM("m_profileLevel->profile = " << m_profileLevel->profile << ", m_profileLevel->level = "
                            << m_profileLevel->level << ", mfx.CodecProfile = " << m_mfxVideoParamsConfig.mfx.CodecProfile
                            << ", mfx.CodecLevel = " << m_mfxVideoParamsConfig.mfx.CodecLevel);
                break;
            }
            case kParamIndexGop: {
                uint32_t syncInterval = 1;
                uint32_t iInterval = 1;   // unused
                uint32_t maxBframes = 0;  // unused
                ParseGop(m_gop, syncInterval, iInterval, maxBframes);
                if (syncInterval > 0) {
                    MFX_DEBUG_TRACE_PRINTF("updating m_mfxVideoParamsConfig.mfx.IdrInterval from %d to %d",
                        m_mfxVideoParamsConfig.mfx.IdrInterval, syncInterval);
                    m_mfxVideoParamsConfig.mfx.IdrInterval = syncInterval;
                }
                break;
            }
            case kParamIndexRequestSyncFrame: {
                if (m_requestSync->value) {
                    MFX_DEBUG_TRACE_MSG("Got sync request");
                    auto update = [this] () {
                        EncoderControl::ModifyFunction modify = [this] (mfxEncodeCtrl* ctrl) {
                            if (m_encoderType == ENCODER_H264 || m_encoderType == ENCODER_H265)
                                ctrl->FrameType = MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I;
                            else
                                ctrl->FrameType = MFX_FRAMETYPE_I;
                        };
                        m_encoderControl.Modify(modify);
                    };

                    if (queue_update) {
                        m_workingQueue.Push(std::move(update));
                    } else {
                        update();
                    }
                }
                break;
            }
            case kParamIndexSyncFrameInterval: {
                if (m_syncFramePeriod->value >= 0) {
                    uint32_t gop_size = getSyncFramePeriod_l(m_syncFramePeriod->value);
                    MFX_DEBUG_TRACE_PRINTF("updating m_mfxVideoParamsConfig.mfx.GopPicSize from %d to %d",
                        m_mfxVideoParamsConfig.mfx.GopPicSize, gop_size);
                    m_mfxVideoParamsConfig.mfx.GopPicSize = gop_size;
                }
                break;
            }
            case kParamIndexColorAspects: {
                if (C2StreamColorAspectsInfo::input::PARAM_TYPE == param->index()) {

                    // set video signal info
                    setColorAspects_l();

                    MFX_DEBUG_TRACE_U32(m_colorAspects->range);
                    MFX_DEBUG_TRACE_U32(m_colorAspects->primaries);
                    MFX_DEBUG_TRACE_U32(m_colorAspects->transfer);
                    MFX_DEBUG_TRACE_U32(m_colorAspects->matrix);
                } else {

                    MFX_DEBUG_TRACE_U32(m_codedColorAspects->range);
                    MFX_DEBUG_TRACE_U32(m_codedColorAspects->primaries);
                    MFX_DEBUG_TRACE_U32(m_codedColorAspects->transfer);
                    MFX_DEBUG_TRACE_U32(m_codedColorAspects->matrix);
                }
                break;
            }
            default:
                break;
        }
    }
}

c2_status_t MfxC2EncoderComponent::UpdateC2ParamToMfx(std::unique_lock<std::mutex> m_statelock,
    const std::vector<C2Param*> &params,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures) {

    (void)m_statelock;
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if (nullptr == failures) {
            res = C2_CORRUPTED; break;
        }

        failures->clear();

        std::lock_guard<std::mutex> lock(m_initEncoderMutex);

        DoUpdateMfxParam(params, failures, true);

        res = GetAggregateStatus(failures);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2EncoderComponent::Queue(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& work : *items) {

        bool eos = (work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
        bool empty = (work->input.buffers.size() == 0) || !work->input.buffers[0];
        MFX_DEBUG_TRACE_STREAM(NAMED(eos) << NAMED(empty));

        if (empty) {
            if (eos) {
                m_workingQueue.Push( [work = std::move(work), this] () mutable {
                    Drain(std::move(work));
                });
            } else {
                MFX_DEBUG_TRACE_MSG("Empty work without EOS flag, return back.");
                ReturnEmptyWork(std::move(work), C2_OK);
            }
        } else {
            m_workingQueue.Push( [ work = std::move(work), this ] () mutable {
                DoWork(std::move(work));
            } );

            if(eos /*|| !m_pendingWorks.empty()*/) {
                m_workingQueue.Push( [this] () { Drain(nullptr); } );
            }
        }
    }

    return C2_OK;
}

void MfxC2EncoderComponent::setColorAspects_l()
{
    MFX_DEBUG_TRACE_FUNC;

    android::ColorAspects sfAspects;

    if (!C2Mapper::map(m_colorAspects->primaries, &sfAspects.mPrimaries)) {
        sfAspects.mPrimaries = android::ColorAspects::PrimariesUnspecified;
    }
    if (!C2Mapper::map(m_colorAspects->range, &sfAspects.mRange)) {
        sfAspects.mRange = android::ColorAspects::RangeUnspecified;
    }
    if (!C2Mapper::map(m_colorAspects->matrix, &sfAspects.mMatrixCoeffs)) {
        sfAspects.mMatrixCoeffs = android::ColorAspects::MatrixUnspecified;
    }
    if (!C2Mapper::map(m_colorAspects->transfer, &sfAspects.mTransfer)) {
        sfAspects.mTransfer = android::ColorAspects::TransferUnspecified;
    }

    int32_t primaries, transfer, matrixCoeffs;
    bool range;
    ColorUtils::convertCodecColorAspectsToIsoAspects(sfAspects,
            &primaries,
            &transfer,
            &matrixCoeffs,
            &range);

    m_signalInfo.ColourDescriptionPresent = 1;
    m_signalInfo.VideoFullRange = range;
    m_signalInfo.TransferCharacteristics = transfer;
    m_signalInfo.MatrixCoefficients= matrixCoeffs;
    m_signalInfo.ColourPrimaries = primaries;

    MFX_DEBUG_TRACE_U32(m_signalInfo.VideoFullRange);
    MFX_DEBUG_TRACE_U32(m_signalInfo.ColourPrimaries);
    MFX_DEBUG_TRACE_U32(m_signalInfo.TransferCharacteristics);
    MFX_DEBUG_TRACE_U32(m_signalInfo.MatrixCoefficients);
    MFX_DEBUG_TRACE_U32(m_signalInfo.ColourDescriptionPresent);
}

std::shared_ptr<C2StreamColorAspectsInfo::output> MfxC2EncoderComponent::getCodedColorAspects_l()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;
    c2_status_t c2_sts = C2_OK;

    mfxExtVideoSignalInfo* vsi{};
    MfxVideoParamsWrapper video_param {};
    std::shared_ptr<C2StreamColorAspectsInfo::output> codedColorAspects = std::make_shared<C2StreamColorAspectsInfo::output>();;

    try {
        vsi = video_param.AddExtBuffer<mfxExtVideoSignalInfo>();

        mfx_res = m_mfxEncoder->GetVideoParam(&video_param);
        MFX_DEBUG_TRACE_U32(vsi->VideoFormat);
        MFX_DEBUG_TRACE_U32(vsi->VideoFullRange);
        MFX_DEBUG_TRACE_U32(vsi->ColourPrimaries);
        MFX_DEBUG_TRACE_U32(vsi->TransferCharacteristics);
        MFX_DEBUG_TRACE_U32(vsi->MatrixCoefficients);
        MFX_DEBUG_TRACE_U32(vsi->ColourDescriptionPresent);
    } catch(std::exception err) {
        MFX_DEBUG_TRACE_STREAM("Error:" << err.what());
        mfx_res = MFX_ERR_MEMORY_ALLOC;
    }

    if (MFX_ERR_NONE == mfx_res) {
        android::ColorAspects sfAspects;

        ColorUtils::convertIsoColorAspectsToCodecAspects(
            vsi->ColourPrimaries,
            vsi->TransferCharacteristics,
            vsi->MatrixCoefficients,
            vsi->VideoFullRange,
            sfAspects);

        MFX_DEBUG_TRACE_U32(sfAspects.mPrimaries);
        MFX_DEBUG_TRACE_U32(sfAspects.mMatrixCoeffs);
        MFX_DEBUG_TRACE_U32(sfAspects.mTransfer);
        MFX_DEBUG_TRACE_U32(sfAspects.mRange);

        if (!C2Mapper::map(sfAspects.mPrimaries, &codedColorAspects->primaries)) {
            codedColorAspects->primaries = C2Color::PRIMARIES_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mRange, &codedColorAspects->range)) {
            codedColorAspects->range = C2Color::RANGE_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mMatrixCoeffs, &codedColorAspects->matrix)) {
            codedColorAspects->matrix = C2Color::MATRIX_UNSPECIFIED;
        }
        if (!C2Mapper::map(sfAspects.mTransfer, &codedColorAspects->transfer)) {
            codedColorAspects->transfer = C2Color::TRANSFER_UNSPECIFIED;
        }

        MFX_DEBUG_TRACE_U32(codedColorAspects->range);
        MFX_DEBUG_TRACE_U32(codedColorAspects->primaries);
        MFX_DEBUG_TRACE_U32(codedColorAspects->transfer);
        MFX_DEBUG_TRACE_U32(codedColorAspects->matrix);
    } else {
        c2_sts = C2_BAD_VALUE;
        MFX_LOG_ERROR("Cannot get color aspects info");
    }

    return codedColorAspects;
}

bool MfxC2EncoderComponent::CodedColorAspectsDiffer(std::shared_ptr<C2StreamColorAspectsInfo::output> vuiColorAspects)
{
    MFX_DEBUG_TRACE_FUNC;

    bool differ = false;
    if (vuiColorAspects->primaries != m_codedColorAspects->primaries ||
        vuiColorAspects->matrix != m_codedColorAspects->matrix ||
        vuiColorAspects->transfer != m_codedColorAspects->transfer ||
        vuiColorAspects->range != m_codedColorAspects->range) {
        differ = true;
    }

    MFX_DEBUG_TRACE_U32(differ);
    return differ;
}

uint32_t MfxC2EncoderComponent::getSyncFramePeriod_l(int32_t sync_frame_period) const
{
    MFX_DEBUG_TRACE_FUNC;

    if (sync_frame_period < 0 || sync_frame_period == INT64_MAX) {
        return 0;
    }

    double frame_rate = m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtN / m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtD * 1.0;
    double period = sync_frame_period / 1e6 * frame_rate;

    MFX_DEBUG_TRACE_F64(frame_rate);
    MFX_DEBUG_TRACE_F64(period);
    return (uint32_t)c2_max(c2_min(period + 0.5, double(UINT32_MAX)), 1.);
}
