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

#include "mfx_c2_decoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"
#include "mfx_c2_allocator_id.h"
#include "mfx_c2_buffer_queue.h"
#include "C2PlatformSupport.h"
#include "mfx_gralloc_allocator.h"

#include <C2AllocatorGralloc.h>
#include <Codec2Mapper.h>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_decoder_component"

const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

enum VP8_PROFILE {
    PROFILE_VP8_0 = C2_PROFILE_LEVEL_VENDOR_START,
};

enum VP8_LEVEL {
    LEVEL_VP8_Version0 = C2_PROFILE_LEVEL_VENDOR_START,
};

MfxC2DecoderComponent::MfxC2DecoderComponent(const C2String name, const CreateConfig& config,
    std::shared_ptr<MfxC2ParamReflector> reflector, DecoderType decoder_type) :
        MfxC2Component(name, config, std::move(reflector)),
        decoder_type_(decoder_type),
        initialized_(false),
        synced_points_count_(0),
        set_hdr_sei_(false)
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2ParamStorage& pr = param_storage_;

    pr.RegisterParam<C2MemoryTypeSetting>("MemoryType");
    pr.RegisterParam<C2PortAllocatorsTuning::output>(C2_PARAMKEY_OUTPUT_ALLOCATORS);
    pr.RegisterParam<C2PortBlockPoolsTuning::output>(C2_PARAMKEY_OUTPUT_BLOCK_POOLS);

    pr.AddValue(C2_PARAMKEY_COMPONENT_DOMAIN,
        std::make_unique<C2ComponentDomainSetting>(C2Component::DOMAIN_VIDEO));

    pr.AddValue(C2_PARAMKEY_COMPONENT_KIND,
        std::make_unique<C2ComponentKindSetting>(C2Component::KIND_DECODER));

    pr.AddValue(C2_PARAMKEY_COMPONENT_NAME,
        AllocUniqueString<C2ComponentNameSetting>(name.c_str()));

    const unsigned int SINGLE_STREAM_ID = 0u;
    pr.AddValue(C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE,
        std::make_unique<C2StreamBufferTypeSetting::input>(SINGLE_STREAM_ID, C2BufferData::LINEAR));
    pr.AddValue(C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE,
        std::make_unique<C2StreamBufferTypeSetting::output>(SINGLE_STREAM_ID, C2BufferData::GRAPHIC));

    pr.AddStreamInfo<C2StreamPictureSizeInfo::output>(
        C2_PARAMKEY_PICTURE_SIZE, SINGLE_STREAM_ID,
        [this] (C2StreamPictureSizeInfo::output* dst)->bool {
            MFX_DEBUG_TRACE("GetPictureSize");
            // Called from Query, video_params_ is already protected there with lock on init_decoder_mutex_
            dst->width = video_params_.mfx.FrameInfo.Width;
            dst->height = video_params_.mfx.FrameInfo.Height;
            MFX_DEBUG_TRACE_STREAM(NAMED(dst->width) << NAMED(dst->height));
            return true;
        },
        [this] (const C2StreamPictureSizeInfo::output& src)->bool {
            MFX_DEBUG_TRACE("SetPictureSize");
            MFX_DEBUG_TRACE_STREAM(NAMED(src.width) << NAMED(src.height));
            // Called from Config, video_params_ is already protected there with lock on init_decoder_mutex_
            video_params_.mfx.FrameInfo.Width = src.width;
            video_params_.mfx.FrameInfo.Height = src.height;
            return true;
        }
    );

    pr.AddStreamInfo<C2StreamCropRectInfo::output>(
        C2_PARAMKEY_CROP_RECT, SINGLE_STREAM_ID,
        [this] (C2StreamCropRectInfo::output* dst)->bool {
            MFX_DEBUG_TRACE("GetCropRect");
            // Called from Query, video_params_ is already protected there with lock on init_decoder_mutex_
            dst->width = video_params_.mfx.FrameInfo.CropW = 640; // default width
            dst->height = video_params_.mfx.FrameInfo.CropH = 480; // default height
            dst->left = video_params_.mfx.FrameInfo.CropX = 0;
            dst->top = video_params_.mfx.FrameInfo.CropY = 0;
            MFX_DEBUG_TRACE_STREAM(NAMED(dst->left) << NAMED(dst->top) <<
                NAMED(dst->width) << NAMED(dst->height));
            return true;
        },
        [this] (const C2StreamCropRectInfo::output& src)->bool {
            MFX_DEBUG_TRACE("SetCropRect");
            // Called from Config, video_params_ is already protected there with lock on init_decoder_mutex_
            MFX_DEBUG_TRACE_STREAM(NAMED(src.left) << NAMED(src.top) <<
                NAMED(src.width) << NAMED(src.height));
            video_params_.mfx.FrameInfo.CropW = src.width;
            video_params_.mfx.FrameInfo.CropH = src.height;
            video_params_.mfx.FrameInfo.CropX = src.left;
            video_params_.mfx.FrameInfo.CropY = src.top;
            return true;
        }
    );

    std::vector<C2Config::profile_t> supported_profiles = {};
    std::vector<C2Config::level_t> supported_levels = {};

    switch(decoder_type_) {
        case DECODER_H264: {
            supported_profiles = {
                PROFILE_AVC_CONSTRAINED_BASELINE,
                PROFILE_AVC_BASELINE,
                PROFILE_AVC_MAIN,
                PROFILE_AVC_CONSTRAINED_HIGH,
                PROFILE_AVC_PROGRESSIVE_HIGH,
                PROFILE_AVC_HIGH,
            };
            supported_levels = {
                LEVEL_AVC_1, LEVEL_AVC_1B, LEVEL_AVC_1_1,
                LEVEL_AVC_1_2, LEVEL_AVC_1_3,
                LEVEL_AVC_2, LEVEL_AVC_2_1, LEVEL_AVC_2_2,
                LEVEL_AVC_3, LEVEL_AVC_3_1, LEVEL_AVC_3_2,
                LEVEL_AVC_4, LEVEL_AVC_4_1, LEVEL_AVC_4_2,
                LEVEL_AVC_5, LEVEL_AVC_5_1, LEVEL_AVC_5_2,
            };

            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::input>(0u, PROFILE_AVC_CONSTRAINED_BASELINE, LEVEL_AVC_5_2));

            pr.AddValue(C2_PARAMKEY_MAX_PICTURE_SIZE,
                std::make_unique<C2StreamMaxPictureSizeTuning::output>(0u, WIDTH_4K, HEIGHT_4K));

            pr.AddValue(C2_PARAMKEY_INPUT_MEDIA_TYPE,
                    AllocUniqueString<C2PortMediaTypeSetting::input>("video/avc"));

            output_delay_ = /*max_dpb_size*/16 + /*for async depth*/1 + /*for msdk unref in sync part*/1;
            input_delay_ = /*for async depth*/1 + /*for msdk unref in sync part*/1;
            break;
        }
        case DECODER_H265: {
            supported_profiles = {
                PROFILE_HEVC_MAIN,
                PROFILE_HEVC_MAIN_STILL,
                PROFILE_HEVC_MAIN_10,
            };

            supported_levels = {
                LEVEL_HEVC_MAIN_1,
                LEVEL_HEVC_MAIN_2, LEVEL_HEVC_MAIN_2_1,
                LEVEL_HEVC_MAIN_3, LEVEL_HEVC_MAIN_3_1,
                LEVEL_HEVC_MAIN_4, LEVEL_HEVC_MAIN_4_1,
                LEVEL_HEVC_MAIN_5, LEVEL_HEVC_MAIN_5_1,
                LEVEL_HEVC_MAIN_5_2, LEVEL_HEVC_HIGH_4,
                LEVEL_HEVC_HIGH_4_1, LEVEL_HEVC_HIGH_5,
                LEVEL_HEVC_HIGH_5_1, LEVEL_HEVC_HIGH_5_2,
            };

            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::input>(0u, PROFILE_HEVC_MAIN, LEVEL_HEVC_MAIN_5_1));

            pr.AddValue(C2_PARAMKEY_MAX_PICTURE_SIZE,
                std::make_unique<C2StreamMaxPictureSizeTuning::output>(0u, WIDTH_8K, HEIGHT_8K));

            pr.AddValue(C2_PARAMKEY_INPUT_MEDIA_TYPE,
                    AllocUniqueString<C2PortMediaTypeSetting::input>("video/hevc"));

            output_delay_ = /*max_dpb_size*/16 + /*for async depth*/1 + /*for msdk unref in sync part*/1;
            input_delay_ = /*for async depth*/1 + /*for msdk unref in sync part*/1;
            break;
        }
        case DECODER_VP9: {
            supported_profiles = {
                PROFILE_VP9_0,
                PROFILE_VP9_2,
            };

            supported_levels = {
                LEVEL_VP9_1, LEVEL_VP9_1_1,
                LEVEL_VP9_2, LEVEL_VP9_2_1,
                LEVEL_VP9_3, LEVEL_VP9_3_1,
                LEVEL_VP9_4, LEVEL_VP9_4_1,
                LEVEL_VP9_5,
            };

            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::input>(0u, PROFILE_VP9_0, LEVEL_VP9_5));

            pr.AddValue(C2_PARAMKEY_MAX_PICTURE_SIZE,
                std::make_unique<C2StreamMaxPictureSizeTuning::output>(0u, WIDTH_8K, HEIGHT_8K));

            pr.AddValue(C2_PARAMKEY_INPUT_MEDIA_TYPE,
                    AllocUniqueString<C2PortMediaTypeSetting::input>("video/x-vnd.on2.vp9"));

            output_delay_ = /*max_dpb_size*/8 + /*for async depth*/1 + /*for msdk unref in sync part*/1;
            input_delay_ = /*for async depth*/1 + /*for msdk unref in sync part*/1;
            break;
        }
        case DECODER_VP8: {
            supported_profiles = {
                (C2Config::profile_t)PROFILE_VP8_0,
            };

            supported_levels = {
                (C2Config::level_t)LEVEL_VP8_Version0,
            };

            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::input>(0u, (C2Config::profile_t)PROFILE_VP8_0, (C2Config::level_t)LEVEL_VP8_Version0));

            pr.AddValue(C2_PARAMKEY_MAX_PICTURE_SIZE,
                std::make_unique<C2StreamMaxPictureSizeTuning::output>(0u, WIDTH_4K, HEIGHT_4K));

            pr.AddValue(C2_PARAMKEY_INPUT_MEDIA_TYPE,
                    AllocUniqueString<C2PortMediaTypeSetting::input>("video/x-vnd.on2.vp8"));

            output_delay_ = /*max_dpb_size*/8 + /*for async depth*/1 + /*for msdk unref in sync part*/1;
            input_delay_ = /*for async depth*/1 + /*for msdk unref in sync part*/1;
            break;
        }
        case DECODER_MPEG2: {
            supported_profiles = {
                PROFILE_MP2V_SIMPLE,
                PROFILE_MP2V_MAIN,
            };

            supported_levels = {
               LEVEL_MP2V_LOW, LEVEL_MP2V_MAIN,
               LEVEL_MP2V_HIGH_1440,
            };

            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::input>(0u, PROFILE_MP2V_MAIN, LEVEL_MP2V_MAIN));

            pr.AddValue(C2_PARAMKEY_MAX_PICTURE_SIZE,
                std::make_unique<C2StreamMaxPictureSizeTuning::output>(0u, WIDTH_2K, HEIGHT_2K));

            pr.AddValue(C2_PARAMKEY_INPUT_MEDIA_TYPE,
                    AllocUniqueString<C2PortMediaTypeSetting::input>("video/mpeg2"));

            output_delay_ = /*max_dpb_size*/4 + /*for async depth*/1 + /*for msdk unref in sync part*/1;
            input_delay_ = /*for async depth*/1 + /*for msdk unref in sync part*/1;
            break;
        }
        case DECODER_AV1: {
            supported_profiles = {
                PROFILE_AV1_0,
                PROFILE_AV1_1,
            };

            supported_levels = {
                LEVEL_AV1_2, LEVEL_AV1_2_1,
                LEVEL_AV1_2_1, LEVEL_AV1_2_3,
                LEVEL_AV1_3, LEVEL_AV1_3_1,
                LEVEL_AV1_3_2,
            };

            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::input>(0u, PROFILE_AV1_0, LEVEL_AV1_2_1));

            pr.AddValue(C2_PARAMKEY_MAX_PICTURE_SIZE,
                std::make_unique<C2StreamMaxPictureSizeTuning::output>(0u, WIDTH_8K, HEIGHT_8K));

            pr.AddValue(C2_PARAMKEY_INPUT_MEDIA_TYPE,
                    AllocUniqueString<C2PortMediaTypeSetting::input>("video/av01"));

            output_delay_ = /*max_dpb_size*/8 + /*for async depth*/1 + /*for msdk unref in sync part*/1;
            input_delay_ = /*for async depth*/1 + /*for msdk unref in sync part*/1;
            break;
        }

        default:
            MFX_DEBUG_TRACE_STREAM("C2PortDelayTuning::output value is not customized which can lead to hangs:" << output_delay_);
            MFX_DEBUG_TRACE_STREAM("C2PortDelayTuning::input value is not customized which can lead to hangs:" << input_delay_);
            break;
    }

    // C2PortDelayTuning::output parameter is needed to say framework about the max delay expressed in
    // decoded frames. If parameter is set too low, framework will stop sanding new portions
    // of bitstream and will wait for decoded frames.
    // The parameter value is differet for codecs and must be equal the DPD value is gotten
    // form QueryIOSurf function call result.
    pr.AddValue(C2_PARAMKEY_OUTPUT_DELAY, std::make_unique<C2PortDelayTuning::output>(output_delay_));

    // The numInputSlots = inputDelayValue + pipelineDelayValue + kSmoothnessFactor;
    // pipelineDelayValue is 0, and kSmoothnessFactor is 4, for 4k video the first frame need 6 input
    pr.AddValue(C2_PARAMKEY_INPUT_DELAY, std::make_unique<C2PortDelayTuning::input>(input_delay_));

    // List all the supported profiles and levels
    pr.RegisterSupportedValues<C2StreamProfileLevelInfo>(&C2StreamProfileLevelInfo::C2ProfileLevelStruct::profile, supported_profiles);
    pr.RegisterSupportedValues<C2StreamProfileLevelInfo>(&C2StreamProfileLevelInfo::C2ProfileLevelStruct::level, supported_levels);

    // Default color aspects
    pr.AddValue(C2_PARAMKEY_DEFAULT_COLOR_ASPECTS, std::make_unique<C2StreamColorAspectsTuning::output>(0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED));
    pr.RegisterSupportedRange<C2StreamColorAspectsTuning>(&C2StreamColorAspectsTuning::C2ColorAspectsStruct::range, RANGE_UNSPECIFIED, RANGE_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsTuning>(&C2StreamColorAspectsTuning::C2ColorAspectsStruct::primaries, PRIMARIES_UNSPECIFIED, PRIMARIES_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsTuning>(&C2StreamColorAspectsTuning::C2ColorAspectsStruct::transfer, TRANSFER_UNSPECIFIED, TRANSFER_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsTuning>(&C2StreamColorAspectsTuning::C2ColorAspectsStruct::matrix, MATRIX_UNSPECIFIED, MATRIX_OTHER);

    // VUI color aspects
    pr.AddValue(C2_PARAMKEY_VUI_COLOR_ASPECTS, std::make_unique<C2StreamColorAspectsInfo::input>(0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED));
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::range, RANGE_UNSPECIFIED, RANGE_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::primaries, PRIMARIES_UNSPECIFIED, PRIMARIES_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::transfer, TRANSFER_UNSPECIFIED, TRANSFER_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::matrix, MATRIX_UNSPECIFIED, MATRIX_OTHER);

    // Color aspects
    pr.AddValue(C2_PARAMKEY_COLOR_ASPECTS, std::make_unique<C2StreamColorAspectsInfo::output>(0u, C2Color::RANGE_UNSPECIFIED, C2Color::PRIMARIES_UNSPECIFIED,
                C2Color::TRANSFER_UNSPECIFIED, C2Color::MATRIX_UNSPECIFIED));
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::range, RANGE_UNSPECIFIED, RANGE_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::primaries, PRIMARIES_UNSPECIFIED, PRIMARIES_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::transfer, TRANSFER_UNSPECIFIED, TRANSFER_OTHER);
    pr.RegisterSupportedRange<C2StreamColorAspectsInfo>(&C2StreamColorAspectsInfo::C2ColorAspectsStruct::matrix, MATRIX_UNSPECIFIED, MATRIX_OTHER);

    // HDR static
    hdr_static_info_.mastering = {
        .red   = { .x = 0.708,  .y = 0.292 },
        .green = { .x = 0.170,  .y = 0.797 },
        .blue  = { .x = 0.131,  .y = 0.046 },
        .white = { .x = 0.3127, .y = 0.3290 },
        .maxLuminance = 0,
        .minLuminance = 0,
    };
    hdr_static_info_.maxCll = 0;
    hdr_static_info_.maxFall = 0;

    pr.AddStreamInfo<C2StreamHdrStaticInfo::output>(
        C2_PARAMKEY_HDR_STATIC_INFO, SINGLE_STREAM_ID,
        [this] (C2StreamHdrStaticInfo::output* dst)->bool {
            MFX_DEBUG_TRACE("GetHdrStaticInfo");
            dst->mastering.red.x = hdr_static_info_.mastering.red.x;
            dst->mastering.red.y = hdr_static_info_.mastering.red.y;
            dst->mastering.green.x = hdr_static_info_.mastering.green.x;
            dst->mastering.green.y = hdr_static_info_.mastering.green.y;
            dst->mastering.blue.x = hdr_static_info_.mastering.blue.x;
            dst->mastering.blue.y = hdr_static_info_.mastering.blue.y;
            dst->maxCll = hdr_static_info_.maxCll;
            dst->maxFall = hdr_static_info_.maxFall;
            return true;
        },
        [this] (const C2StreamHdrStaticInfo::output& src)->bool {
            MFX_DEBUG_TRACE("SetHdrStaticInfo");
            hdr_static_info_ = src;
            return true;
        }
    );

    param_storage_.DumpParams();
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

    mfxStatus mfx_res = MfxDev::Create(MfxDev::Usage::Decoder, &device_);

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

c2_status_t MfxC2DecoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    synced_points_count_ = 0;
    mfxStatus mfx_res = MFX_ERR_NONE;
    eos_received_ = false;

    do {
        bool allocator_required = (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY);

        if (allocator_required != allocator_set_) {

            mfx_res = session_.Close();
            if (MFX_ERR_NONE != mfx_res) break;

            mfx_res = InitSession();
            if (MFX_ERR_NONE != mfx_res) break;

            // set frame allocator
            if (allocator_required) {
                allocator_ = device_->GetFramePoolAllocator();
                mfx_res = session_.SetFrameAllocator(&(device_->GetFrameAllocator()->GetMfxAllocator()));

            } else {
                allocator_ = nullptr;
                mfx_res = session_.SetFrameAllocator(nullptr);
            }
            if (MFX_ERR_NONE != mfx_res) break;

            allocator_set_ = allocator_required;
        }

        MFX_DEBUG_TRACE_STREAM(surfaces_.size());

        working_queue_.Start();
        waiting_queue_.Start();

    } while(false);

    return C2_OK;
}

c2_status_t MfxC2DecoderComponent::DoStop(bool abort)
{
    MFX_DEBUG_TRACE_FUNC;

    // Working queue should stop first otherwise race condition
    // is possible when waiting queue is stopped (first), but working
    // queue is pushing tasks into it (via DecodeFrameAsync). As a
    // result, such tasks will be processed after next start
    // which is bad as sync point becomes invalid after
    // decoder Close/Init.
    if (abort) {
        working_queue_.Abort();
        waiting_queue_.Abort();
    } else {
        working_queue_.Stop();
        waiting_queue_.Stop();
    }

    {
        std::lock_guard<std::mutex> lock(pending_works_mutex_);
        for (auto& pair : pending_works_) {
            // Other statuses cause libstagefright_ccodec fatal error
            NotifyWorkDone(std::move(pair.second), C2_NOT_FOUND);
        }
        pending_works_.clear();
    }

    c2_allocator_ = nullptr;

    FreeDecoder();

    // c2_bitstream_->Reset() doesn't cleanup header stored in frame constructor
    // that causes test influence to each other
    // and false positive, so re-create it.
    InitFrameConstructor();

    return C2_OK;
}

c2_status_t MfxC2DecoderComponent::Release()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    mfxStatus sts = session_.Close();
    if (MFX_ERR_NONE != sts) res = MfxStatusToC2(sts);

    if (device_) {
        device_->Close();
        if (MFX_ERR_NONE != sts) res = MfxStatusToC2(sts);

        device_ = nullptr;
    }
    return res;
}

void MfxC2DecoderComponent::InitFrameConstructor()
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2FrameConstructorType fc_type;
    switch (decoder_type_)
    {
    case DECODER_H264:
        fc_type = MfxC2FC_AVC;
        break;
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
    c2_bitstream_ = std::make_unique<MfxC2BitstreamIn>(fc_type);
}

mfxStatus MfxC2DecoderComponent::InitSession()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    do {
        mfx_res = session_.Init(mfx_implementation_, &g_required_mfx_version);
        if (MFX_ERR_NONE != mfx_res) {
            MFX_DEBUG_TRACE_MSG("MFXVideoSession::Init failed");
            break;
        }
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Major);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Minor);

        mfx_res = session_.QueryIMPL(&mfx_implementation_);
        if (MFX_ERR_NONE != mfx_res) break;
        MFX_DEBUG_TRACE_I32(mfx_implementation_);

        mfx_res = device_->InitMfxSession(&session_);

    } while (false);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2DecoderComponent::ResetSettings()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;
    memset(&video_params_, 0, sizeof(mfxVideoParam));

    memset(&signal_info_, 0, sizeof(mfxExtVideoSignalInfo));
    signal_info_.Header.BufferId = MFX_EXTBUFF_VIDEO_SIGNAL_INFO;
    signal_info_.Header.BufferSz = sizeof(mfxExtVideoSignalInfo);

    switch (decoder_type_)
    {
    case DECODER_H264:
        video_params_.mfx.CodecId = MFX_CODEC_AVC;
        break;
    case DECODER_H265:
        video_params_.mfx.CodecId = MFX_CODEC_HEVC;
        break;
    case DECODER_VP9:
        video_params_.mfx.CodecId = MFX_CODEC_VP9;
        break;
	case DECODER_VP8:
        video_params_.mfx.CodecId = MFX_CODEC_VP8;
        break;
    case DECODER_MPEG2:
        video_params_.mfx.CodecId = MFX_CODEC_MPEG2;
        break;
    case DECODER_AV1:
        video_params_.mfx.CodecId = MFX_CODEC_AV1;
        break;
    default:
        MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
        break;
    }

    color_aspects_.SetCodecID(video_params_.mfx.CodecId);

    mfx_set_defaults_mfxVideoParam_dec(&video_params_);

    if (device_)
    {
        // default pattern: video memory if allocator available
        video_params_.IOPattern = device_->GetFrameAllocator() ?
        MFX_IOPATTERN_OUT_VIDEO_MEMORY : MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

    }
    else
    {
        res = MFX_ERR_NULL_PTR;
    }

    return res;
}

mfxU16 MfxC2DecoderComponent::GetAsyncDepth(void)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxU16 asyncDepth;
    if ((MFX_IMPL_HARDWARE == MFX_IMPL_BASETYPE(mfx_implementation_)) &&
        ((MFX_CODEC_AVC == video_params_.mfx.CodecId) ||
         (MFX_CODEC_HEVC == video_params_.mfx.CodecId) ||
         (MFX_CODEC_VP9 == video_params_.mfx.CodecId) ||
         (MFX_CODEC_VP8 == video_params_.mfx.CodecId) ||
         (MFX_CODEC_AV1 == video_params_.mfx.CodecId) ||
         (MFX_CODEC_MPEG2 == video_params_.mfx.CodecId)))
        asyncDepth = 1;
    else
        asyncDepth = 0;

    MFX_DEBUG_TRACE_I32(asyncDepth);
    return asyncDepth;
}

mfxStatus MfxC2DecoderComponent::InitDecoder(std::shared_ptr<C2BlockPool> c2_allocator)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    std::lock_guard<std::mutex> lock(init_decoder_mutex_);

    {
        MFX_DEBUG_TRACE_MSG("InitDecoder: DecodeHeader");

        if (nullptr == decoder_) {
            decoder_.reset(MFX_NEW_NO_THROW(MFXVideoDECODE(session_)));
            if (nullptr == decoder_) {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        }

        if (MFX_ERR_NONE == mfx_res) {
            // saving parameters
            mfxVideoParam oldParams = video_params_;

            ext_buffers_.push_back(reinterpret_cast<mfxExtBuffer*>(&signal_info_));
            video_params_.NumExtParam = ext_buffers_.size();
            video_params_.ExtParam = &ext_buffers_.front();

            // decoding header
            mfx_res = decoder_->DecodeHeader(c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get(), &video_params_);

            ext_buffers_.pop_back();
            video_params_.NumExtParam = oldParams.NumExtParam;
            video_params_.ExtParam = oldParams.ExtParam;
        }
        if (MFX_ERR_NULL_PTR == mfx_res) {
            mfx_res = MFX_ERR_MORE_DATA;
        }

        video_params_.mfx.FrameInfo.Width = MFX_MEM_ALIGN(video_params_.mfx.FrameInfo.Width, 16);
        video_params_.mfx.FrameInfo.Height = MFX_MEM_ALIGN(video_params_.mfx.FrameInfo.Height, 16);
        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = c2_bitstream_->GetFrameConstructor()->Init(video_params_.mfx.CodecProfile, video_params_.mfx.FrameInfo);
        }
    }
    if (MFX_ERR_NONE == mfx_res) {
        MFX_DEBUG_TRACE_MSG("InitDecoder: UpdateBitstreamColorAspects");
        color_aspects_.UpdateBitstreamColorAspects(signal_info_);

        MFX_DEBUG_TRACE_MSG("InitDecoder: UpdateHdrStaticInfo");
        UpdateHdrStaticInfo();

        MFX_DEBUG_TRACE_MSG("InitDecoder: GetAsyncDepth");
        video_params_.AsyncDepth = GetAsyncDepth();
    }

    //We need check whether the BQ allocator has a surface, if No we cannot use MFX_IOPATTERN_OUT_VIDEO_MEMORY mode.
    if (MFX_ERR_NONE == mfx_res && video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
        std::shared_ptr<C2GraphicBlock> out_block;
        c2_status_t res = C2_OK;
        C2MemoryUsage mem_usage = {C2AndroidMemoryUsage::CPU_READ|C2AndroidMemoryUsage::HW_COMPOSER_READ,
                                     C2AndroidMemoryUsage::HW_CODEC_WRITE};

        res = c2_allocator_->fetchGraphicBlock(video_params_.mfx.FrameInfo.Width,
                                           video_params_.mfx.FrameInfo.Height,
                                           MfxFourCCToGralloc(video_params_.mfx.FrameInfo.FourCC),
                                           mem_usage, &out_block);

        if (res == C2_OK) {
            uint32_t width, height, format, stride, igbp_slot, generation;
            uint64_t usage, igbp_id;
            android::_UnwrapNativeCodec2GrallocMetadata( out_block->handle(), &width, &height, &format, &usage,
                                                    &stride, &generation, &igbp_id, &igbp_slot);
            if (!igbp_id && !igbp_slot) {
                //No surface & BQ
                video_params_.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
                allocator_ = nullptr;
                mfx_res= session_.SetFrameAllocator(nullptr);
                allocator_set_ = false;
                ALOGI("Format = 0x%x. System memory is being used for decoding!", format);
                if (MFX_ERR_NONE != mfx_res) MFX_DEBUG_TRACE_MSG("SetFrameAllocator failed");
            }
        }
    }

    if (MFX_ERR_NONE == mfx_res) {
        MFX_DEBUG_TRACE_MSG("InitDecoder: Init");

        MFX_DEBUG_TRACE__mfxVideoParam_dec(video_params_);

        if (allocator_) {
            allocator_->SetC2Allocator(c2_allocator);
            allocator_->SetBufferCount(output_delay_);
        }

        MFX_DEBUG_TRACE_MSG("Decoder initializing...");
        mfx_res = decoder_->Init(&video_params_);
        MFX_DEBUG_TRACE_PRINTF("Decoder initialized, sts = %d", mfx_res);
        // c2 allocator is needed to handle mfxAllocRequest coming from decoder_->Init,
        // not needed after that.
        if (allocator_) allocator_->SetC2Allocator(nullptr);

        if (MFX_WRN_PARTIAL_ACCELERATION == mfx_res) {
            MFX_DEBUG_TRACE_MSG("InitDecoder returns MFX_WRN_PARTIAL_ACCELERATION");
            mfx_res = MFX_ERR_NONE;
        } else if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfx_res) {
            MFX_DEBUG_TRACE_MSG("InitDecoder returns MFX_WRN_INCOMPATIBLE_VIDEO_PARAM");
            mfx_res = MFX_ERR_NONE;
        }
        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = decoder_->GetVideoParam(&video_params_);
            max_width_ = video_params_.mfx.FrameInfo.Width;
            max_height_ = video_params_.mfx.FrameInfo.Height;

            MFX_DEBUG_TRACE__mfxVideoParam_dec(video_params_);
        }
        if (MFX_ERR_NONE == mfx_res) {
            initialized_ = true;
        }
    }
    if (MFX_ERR_NONE != mfx_res) {
        FreeDecoder();
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

void MfxC2DecoderComponent::FreeDecoder()
{
    MFX_DEBUG_TRACE_FUNC;

    initialized_ = false;

    locked_surfaces_.clear();
    locked_block_.clear();

    if(nullptr != decoder_) {
        decoder_->Close();
        decoder_ = nullptr;
    }

    max_height_ = 0;
    max_width_ = 0;

    surfaces_.clear();

    if (allocator_) {
        allocator_->Reset();
    }
}

c2_status_t MfxC2DecoderComponent::QueryParam(const mfxVideoParam* src, C2Param::Index index, C2Param** dst) const
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    res = param_storage_.QueryParam(index, dst);
    if (C2_NOT_FOUND == res) {
        switch (index) {
            case kParamIndexMemoryType: {
                if (nullptr == *dst) {
                    *dst = new C2MemoryTypeSetting();
                }

                C2MemoryTypeSetting* setting = static_cast<C2MemoryTypeSetting*>(*dst);
                if (!MfxIOPatternToC2MemoryType(false, src->IOPattern, &setting->value)) res = C2_CORRUPTED;
                break;
            }
            case C2PortAllocatorsTuning::output::PARAM_TYPE: {
                if (nullptr == *dst) {
                    std::unique_ptr<C2PortAllocatorsTuning::output> outAlloc =
                        C2PortAllocatorsTuning::output::AllocUnique(1);
                    if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY)
#ifdef MFX_BUFFER_QUEUE
                        outAlloc->m.values[0] = MFX_BUFFERQUEUE;
#else
                        outAlloc->m.values[0] = C2PlatformAllocatorStore::GRALLOC;
#endif
                    else
                        outAlloc->m.values[0] = C2PlatformAllocatorStore::GRALLOC;
                    MFX_DEBUG_TRACE_PRINTF("Set output port alloctor to: %d", outAlloc->m.values[0]);
                    *dst = outAlloc.release();
                    res = C2_OK;
                } else {
                    // It is not possible to return flexible params through stack
                    res = C2_NO_MEMORY;
                }
                break;
            }
            default:
                res = C2_BAD_INDEX;
                break;
        }
    }

    return res;
}

c2_status_t MfxC2DecoderComponent::Query(
    std::unique_lock<std::mutex> state_lock,
    const std::vector<C2Param*> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    (void)state_lock;
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(init_decoder_mutex_);

    c2_status_t res = C2_OK;

    // determine source, update it if needed
    const mfxVideoParam* params_view = &video_params_;
    if (nullptr != params_view) {
        // 1st cycle on stack params
        for (C2Param* param : stackParams) {
            c2_status_t param_res = C2_OK;
            if (param_storage_.FindParam(param->index())) {
                param_res = QueryParam(params_view, param->index(), &param);
            } else {
                param_res =  C2_BAD_INDEX;
            }
            if (param_res != C2_OK) {
                param->invalidate();
                res = param_res;
            }
        }
        // 2nd cycle on heap params
        for (C2Param::Index param_index : heapParamIndices) {
            // allocate in QueryParam
            C2Param* param = nullptr;
            // check on presence
            c2_status_t param_res = C2_OK;
            if (param_storage_.FindParam(param_index.type())) {
                param_res = QueryParam(params_view, param_index, &param);
            } else {
                param_res = C2_BAD_INDEX;
            }
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

void MfxC2DecoderComponent::DoConfig(const std::vector<C2Param*> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures,
    bool /*queue_update*/)
{
    MFX_DEBUG_TRACE_FUNC;

    for (const C2Param* param : params) {

        // check whether plugin supports this parameter
        std::unique_ptr<C2SettingResult> find_res = param_storage_.FindParam(param);
        if(nullptr != find_res) {
            MFX_DEBUG_TRACE_PRINTF("cannot found param: %X02", param->index());
            failures->push_back(std::move(find_res));
            continue;
        }
        // check whether plugin is in a correct state to apply this parameter
        bool modifiable = ((param->kind() & C2Param::TUNING) != 0) ||
            state_ == State::STOPPED; /* all kinds, even INFO might be set by stagefright */

        if (!modifiable) {
            MFX_DEBUG_TRACE_PRINTF("cannot modify param: %X02", param->index());
            failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::READ_ONLY));
            continue;
        }

        // check ranges
        if(!param_storage_.ValidateParam(param, failures)) {
            continue;
        }

        // applying parameter
        switch (C2Param::Type(param->type()).typeIndex()) {
            case kParamIndexMemoryType: {
                const C2MemoryTypeSetting* setting = static_cast<const C2MemoryTypeSetting*>(param);
                bool set_res = C2MemoryTypeToMfxIOPattern(false, setting->value, &video_params_.IOPattern);
                if (!set_res) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexBlockPools: {
                const C2PortBlockPoolsTuning::output* outputPools =
                                static_cast<const C2PortBlockPoolsTuning::output*>(param);

                if (outputPools && outputPools->flexCount() >= 1) {
                    output_pool_id_ = outputPools->m.values[0];
                    MFX_DEBUG_TRACE_PRINTF("config kParamIndexBlockPools to %d",output_pool_id_);
                }
                else {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexColorAspects: {
                const C2StreamColorAspectsTuning* settings = static_cast<const C2StreamColorAspectsTuning*>(param);
                android::ColorAspects ca;
                MFX_DEBUG_TRACE_U32(settings->range);
                MFX_DEBUG_TRACE_U32(settings->primaries);
                MFX_DEBUG_TRACE_U32(settings->transfer);
                MFX_DEBUG_TRACE_U32(settings->matrix);

                ca.mRange = (android::ColorAspects::Range)settings->range;
                ca.mTransfer = (android::ColorAspects::Transfer)settings->transfer;
                ca.mMatrixCoeffs = (android::ColorAspects::MatrixCoeffs)settings->matrix;
                ca.mPrimaries = (android::ColorAspects::Primaries)settings->primaries;

                mfxExtVideoSignalInfo signal_info;
                signal_info.VideoFullRange = settings->range;
                signal_info.ColourPrimaries = settings->primaries;
                signal_info.TransferCharacteristics = settings->transfer;
                signal_info.MatrixCoefficients = settings->matrix;

                color_aspects_.UpdateBitstreamColorAspects(signal_info);
                color_aspects_.SetFrameworkColorAspects(ca);
                break;
            }

            default:
                MFX_DEBUG_TRACE_PRINTF("applying default parameter: %X02", param->index());
                param_storage_.ConfigParam(*param, state_ == State::STOPPED, failures);
                break;
        }
    }
}

c2_status_t MfxC2DecoderComponent::Config(std::unique_lock<std::mutex> state_lock,
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

        std::lock_guard<std::mutex> lock(init_decoder_mutex_);

        DoConfig(params, failures, true);

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
        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
        dev_busy_cond_.wait(lock, [this] { return synced_points_count_ < video_params_.AsyncDepth; } );
        // Can release the lock here as synced_points_count_ is incremented in this thread,
        // so condition cannot go to false.
    }

    do {
      mfx_res = decoder_->DecodeFrameAsync(bs, surface_work, surface_out, syncp);
      ++trying_count;

      if (MFX_WRN_DEVICE_BUSY == mfx_res) {

        if (flushing_) {
            // break waiting as flushing in progress and return MFX_WRN_DEVICE_BUSY as sign of it
            break;
        }

        if (trying_count >= MAX_TRYING_COUNT) {
            MFX_DEBUG_TRACE_MSG("Too many MFX_WRN_DEVICE_BUSY from DecodeFrameAsync");
            mfx_res = MFX_ERR_DEVICE_FAILED;
            break;
        }

        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
        unsigned int synced_points_count = synced_points_count_;
        // wait for change of synced_points_count_
        // that might help with MFX_WRN_DEVICE_BUSY
        dev_busy_cond_.wait_for(lock, timeout, [this, synced_points_count] {
            return synced_points_count_ < synced_points_count;
        } );
        if (flushing_) { // do check flushing again after timeout to not call DecodeFrameAsync once more
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

        if (MFX_WRN_VIDEO_PARAM_CHANGED == mfx_sts) mfx_sts = MFX_ERR_MORE_SURFACE;

        if ((MFX_ERR_NONE == mfx_sts) || (MFX_ERR_MORE_DATA == mfx_sts) || (MFX_ERR_MORE_SURFACE == mfx_sts)) {

            *expect_output = true; // It might not really produce output frame from consumed bitstream,
            // but neither surface_work->Data.Locked nor mfx_sts provide exact status of that.

            std::unique_lock<std::mutex> lock(locked_surfaces_mutex_);
            // add output to waiting pool in case of Decode success only
            locked_surfaces_.push_back(std::move(frame_work));

            MFX_DEBUG_TRACE_P(surface_out);
            if (nullptr != surface_out) {

                MFX_DEBUG_TRACE_I32(surface_out->Data.Locked);
                MFX_DEBUG_TRACE_I64(surface_out->Data.TimeStamp);

                if (MFX_ERR_NONE == mfx_sts) {

                    auto pred_match_surface =
                        [surface_out] (const auto& item) { return item.GetMfxFrameSurface().get() == surface_out; };

                    MfxC2FrameOut frame_out;
                    auto found = std::find_if(locked_surfaces_.begin(), locked_surfaces_.end(),
                        pred_match_surface);
                    if (found != locked_surfaces_.end()) {
                        frame_out = *found;
                    } else {
                        MFX_DEBUG_TRACE_STREAM("Not found LOCKED!!!");
                        // If not found pass empty frame_out to WaitWork, it will report an error.
                    }
                    lock.unlock(); // unlock the mutex asap

                    waiting_queue_.Push(
                        [ frame = std::move(frame_out), sync_point, this ] () mutable {
                        WaitWork(std::move(frame), sync_point);
                    } );
                    {
                        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
                        ++synced_points_count_;
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

c2_status_t MfxC2DecoderComponent::AllocateC2Block(uint32_t width, uint32_t height, uint32_t fourcc, std::shared_ptr<C2GraphicBlock>* out_block)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {

        if (!c2_allocator_) {
            res = C2_NOT_FOUND;
            break;
        }

        if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {

            C2MemoryUsage mem_usage = {C2AndroidMemoryUsage::CPU_READ|C2AndroidMemoryUsage::HW_COMPOSER_READ,
                                            C2AndroidMemoryUsage::HW_CODEC_WRITE};
            res = c2_allocator_->fetchGraphicBlock(width, height,
                                               MfxFourCCToGralloc(fourcc), mem_usage, out_block);
            if (res == C2_OK) {
                buffer_handle_t hndl = android::UnwrapNativeCodec2GrallocHandle((*out_block)->handle());
                uint64_t id;
                c2_status_t sts = gralloc_allocator_->GetBackingStore(hndl, &id);
                if (allocator_ && !allocator_->InCache(id)) {
                    res = C2_BLOCKING;
                    usleep(1000);
                    MFX_DEBUG_TRACE_PRINTF("fetchGraphicBlock: BLOCKING");
                    ALOGE("fetchGraphicBlock a nocached block, please retune output blocks.");
                }
            }
        } else if (video_params_.IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY) {

            C2MemoryUsage mem_usage = {C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE};

            res = c2_allocator_->fetchGraphicBlock(width, height,
                                               MfxFourCCToGralloc(fourcc, false), mem_usage, out_block);
       }
    } while (res == C2_BLOCKING);

    MFX_DEBUG_TRACE_I32(res);
    return res;
}

c2_status_t MfxC2DecoderComponent::AllocateFrame(MfxC2FrameOut* frame_out)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::shared_ptr<MfxFrameConverter> converter;
    if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
        converter = device_->GetFrameConverter();
    }

    do
    {
        auto pred_unlocked = [&](const MfxC2FrameOut &item) {
            return !item.GetMfxFrameSurface()->Data.Locked;
        };
        {
            std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
            locked_surfaces_.remove_if(pred_unlocked);
        }

        std::shared_ptr<C2GraphicBlock> out_block;
        res = AllocateC2Block(video_params_.mfx.FrameInfo.Width, video_params_.mfx.FrameInfo.Height,
                              video_params_.mfx.FrameInfo.FourCC, &out_block);
        if (C2_TIMED_OUT == res)
        {
            continue;
        }
        if (C2_OK != res) break;

        buffer_handle_t hndl = android::UnwrapNativeCodec2GrallocHandle(out_block->handle());
        auto it = surfaces_.end();
        if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY)
        {
            uint64_t id;
            c2_status_t sts = gralloc_allocator_->GetBackingStore(hndl, &id);

            it = surfaces_.find(id);
            if (it == surfaces_.end()){
                // haven't been used for decoding yet
                res = MfxC2FrameOut::Create(converter, out_block, video_params_.mfx.FrameInfo, TIMEOUT_NS, frame_out, hndl);
                if (C2_OK != res)
                    break;

                surfaces_.emplace(id, frame_out->GetMfxFrameSurface());
            } else {
                if (it->second->Data.Locked) {
                    /* Buffer locked, try next block. */
                    MFX_DEBUG_TRACE_PRINTF("Buffer still locked, try next block");
                    res = C2_TIMED_OUT;
                }
                else
                    *frame_out = MfxC2FrameOut(std::move(out_block), it->second);
            }
        } else {
            res = MfxC2FrameOut::Create(converter, out_block, video_params_.mfx.FrameInfo, TIMEOUT_NS, frame_out, hndl);
            if (C2_OK != res)
            break;
        }
    } while (C2_TIMED_OUT == res);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

void MfxC2DecoderComponent::DoWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    if (flushing_)
    {
        flushed_works_.push_back(std::move(work));
        return;
    }

    c2_status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    const auto incoming_frame_index = work->input.ordinal.frameIndex;
    const auto incoming_flags = work->input.flags;

    MFX_DEBUG_TRACE_STREAM("work: " << work.get() << "; index: " << incoming_frame_index.peeku() <<
        " flags: " << std::hex << incoming_flags);

    bool expect_output = false;
    bool flushing = false;

    do {
        std::unique_ptr<MfxC2BitstreamIn::FrameView> bitstream_view;
        res = c2_bitstream_->AppendFrame(work->input, TIMEOUT_NS, &bitstream_view);
        if (C2_OK != res) break;

        if (work->input.buffers.size() == 0) break;

        PushPending(std::move(work));

        if (!c2_allocator_) {
            res = GetCodec2BlockPool(output_pool_id_,
                shared_from_this(), &c2_allocator_);
            if (res != C2_OK) break;
#ifdef MFX_BUFFER_QUEUE
            bool hasSurface = std::static_pointer_cast<MfxC2BufferQueueBlockPool>(c2_allocator_)->outputSurfaceSet();
            video_params_.IOPattern = hasSurface ? MFX_IOPATTERN_OUT_VIDEO_MEMORY : MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
#endif
            if (video_params_.IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY) {
                allocator_ = nullptr;
                mfx_sts = session_.SetFrameAllocator(nullptr);
                allocator_set_ = false;
                ALOGI("System memory is being used for decoding!");

                if (MFX_ERR_NONE != mfx_sts) break;
            }
        }

        if (!gralloc_allocator_) {
            res = MfxGrallocAllocator::Create(&gralloc_allocator_);
            if(C2_OK != res) {
                break;
            }
        }

        // loop repeats DecodeFrame on the same frame
        // if DecodeFrame returns error which is repairable, like resolution change
        bool resolution_change = false;
        do {
            if (!initialized_) {
                mfx_sts = InitDecoder(c2_allocator_);
                if(MFX_ERR_NONE != mfx_sts) {
                    MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                    if (MFX_ERR_MORE_DATA == mfx_sts) {
                        mfx_sts = MFX_ERR_NONE; // not enough data for InitDecoder should not cause an error
                    }
                    res = MfxStatusToC2(mfx_sts);
                    break;
                }
                if (!initialized_) {
                    MFX_DEBUG_TRACE_MSG("Cannot initialize mfx decoder");
                    res = C2_BAD_VALUE;
                    break;
                }
                if (set_hdr_sei_) {
                    MFX_DEBUG_TRACE_MSG("Set HDR static info");
                    std::lock_guard<std::mutex> lock(pending_works_mutex_);
                    auto it = pending_works_.find(incoming_frame_index);
                    if (it != pending_works_.end()) {
                        it->second->worklets.front()->output.configUpdate.push_back(C2Param::Copy(hdr_static_info_));
                    } else {
                        MFX_DEBUG_TRACE_MSG("Cannot find the Work in Pending works");
                    }
                    set_hdr_sei_ = false;
                }
            }

            mfxBitstream *bs = c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get();
            MfxC2FrameOut frame_out;
            do {
                res = AllocateFrame(&frame_out);
                if (C2_OK != res) break;

                mfx_sts = DecodeFrame(bs, std::move(frame_out), &flushing, &expect_output);
            } while (mfx_sts == MFX_ERR_NONE || mfx_sts == MFX_ERR_MORE_SURFACE);

            if (MFX_ERR_MORE_DATA == mfx_sts) {
                mfx_sts = MFX_ERR_NONE; // valid result of DecodeFrame
            }

            resolution_change = (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mfx_sts);
            if (resolution_change) {

                frame_out = MfxC2FrameOut(); // release the frame to be used in Drain

                Drain(nullptr);

                // Clear up all queue of works after drain except last work
                // which caused resolution change and should be used again.
                {
                    std::lock_guard<std::mutex> lock(pending_works_mutex_);
                    auto it = pending_works_.begin();
                    while (it != pending_works_.end()) {
                        if (it->first != incoming_frame_index) {
                            MFX_DEBUG_TRACE_STREAM("Work removed: " << NAMED(it->second->input.ordinal.frameIndex.peeku()));
                            NotifyWorkDone(std::move(it->second), C2_NOT_FOUND);
                            it = pending_works_.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                bool resolution_change_done = false;

                mfxStatus decode_header_sts = decoder_->DecodeHeader(c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get(), &video_params_);
                MFX_DEBUG_TRACE__mfxStatus(decode_header_sts);
                mfx_sts = decode_header_sts;

                if (!resolution_change_done) {
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

        res = bitstream_view->Release();
        if (C2_OK != res) break;

    } while(false); // fake loop to have a cleanup point there

    bool incomplete_frame =
        (incoming_flags & (C2FrameData::FLAG_INCOMPLETE | C2FrameData::FLAG_CODEC_CONFIG)) != 0;

    // notify listener in case of failure or empty output
    if (C2_OK != res || !expect_output || incomplete_frame || flushing) {
        if (!work) {
            std::lock_guard<std::mutex> lock(pending_works_mutex_);
            auto it = pending_works_.find(incoming_frame_index);
            if (it != pending_works_.end()) {
                work = std::move(it->second);
                pending_works_.erase(it);
            } else {
                MFX_DEBUG_TRACE_STREAM("Not found C2Work to return error result!!!");
                FatalError(C2_CORRUPTED);
            }
        }
        if (work) {
            if (C2_OK == res) {
                std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
                // Pass end of stream flag only.
                worklet->output.flags = (C2FrameData::flags_t)(work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
                worklet->output.ordinal = work->input.ordinal;
            }
            if (flushing) {
                flushed_works_.push_back(std::move(work));
            } else {
                NotifyWorkDone(std::move(work), res);
            }
        }
    }
}

void MfxC2DecoderComponent::Drain(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_sts = MFX_ERR_NONE;

    if (initialized_) {
        do {

            if (flushing_) {
                if (work) {
                    flushed_works_.push_back(std::move(work));
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
                    flushed_works_.push_back(std::move(work));
                }
                break;
            }
        // exit cycle if MFX_ERR_MORE_DATA or critical error happens
        } while (MFX_ERR_NONE == mfx_sts || MFX_ERR_MORE_SURFACE == mfx_sts);

        // eos work, should be sent after last work returned
        if (work) {
            waiting_queue_.Push([work = std::move(work), this]() mutable {

                std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

                worklet->output.flags = (C2FrameData::flags_t)(work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
                worklet->output.ordinal = work->input.ordinal;

                NotifyWorkDone(std::move(work), C2_OK);
            });
        }

        const auto timeout = std::chrono::seconds(10);
        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
        bool cv_res =
            dev_busy_cond_.wait_for(lock, timeout, [this] { return synced_points_count_ == 0; } );
        if (!cv_res) {
            MFX_DEBUG_TRACE_MSG("Timeout on drain completion");
        }
    }
}

void MfxC2DecoderComponent::WaitWork(MfxC2FrameOut&& frame_out, mfxSyncPoint sync_point)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    {
        mfxStatus mfx_res = session_.SyncOperation(sync_point, MFX_TIMEOUT_INFINITE);
        if (MFX_ERR_NONE != mfx_res) {
            MFX_DEBUG_TRACE_MSG("SyncOperation failed");
            MFX_DEBUG_TRACE__mfxStatus(mfx_res);
            res = MfxStatusToC2(mfx_res);
        }
    }

    std::shared_ptr<mfxFrameSurface1> mfx_surface = frame_out.GetMfxFrameSurface();
    MFX_DEBUG_TRACE_I32(mfx_surface->Data.Locked);
    MFX_DEBUG_TRACE_I64(mfx_surface->Data.TimeStamp);

#if MFX_DEBUG_DUMP_FRAME == MFX_DEBUG_YES
    static int frameIndex = 0;
    uint8_t stride = frame_out.GetC2GraphicView()->layout().planes[C2PlanarLayout::PLANE_Y].rowInc;
    static YUVWriter writer("/data/local/tmp",std::vector<std::string>({}),"decoder_frame.log");
    writer.Write(mfx_surface->Data.Y, stride, frame_out.GetC2GraphicBlock()->height(), frameIndex++);
#endif

    decltype(C2WorkOrdinalStruct::timestamp) ready_timestamp{mfx_surface->Data.TimeStamp};

    std::unique_ptr<C2Work> work;

    {
        std::lock_guard<std::mutex> lock(pending_works_mutex_);

        auto it = find_if(pending_works_.begin(), pending_works_.end(), [ready_timestamp] (const auto &item) {
            return item.second->input.ordinal.timestamp == ready_timestamp;
        });

        if (it != pending_works_.end()) {
            work = std::move(it->second);
            pending_works_.erase(it);
        }
    }

    do {

        //C2Event event; // not supported yet, left for future use
        //event.fire(); // pre-fire event as output buffer is ready to use

        const C2Rect rect = C2Rect(mfx_surface->Info.CropW, mfx_surface->Info.CropH)
            .at(mfx_surface->Info.CropX, mfx_surface->Info.CropY);

        {
            // Update frame format description to be returned by Query method
            // through parameters C2StreamPictureSizeInfo and C2StreamCropRectInfo.
            // Although updated frame format is available right after DecodeFrameAsync call,
            // getting update there doesn't give any improvement in terms of mutex sync,
            // as init_decoder_mutex_ is not locked there.

            // Also parameter update here is easily tested by comparison parameter with output in onWorkDone callback.
            // If parameters update is done after DecodeFrameAsync call
            // then it becomes not synchronized with output and input,
            // looks random from client side and cannot be tested.
            std::lock_guard<std::mutex> lock(init_decoder_mutex_);
            video_params_.mfx.FrameInfo = mfx_surface->Info;
        }

        std::shared_ptr<C2GraphicBlock> block = frame_out.GetC2GraphicBlock();
        if (!block) {
            res = C2_CORRUPTED;
            break;
        }

        {
            std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
            // If output is not Locked - we have to release it asap to not interfere with possible
            // frame mapping in onWorkDone callback,
            // if output is Locked - remove it temporarily from locked_surfaces_ to avoid holding
            // locked_surfaces_mutex_ while long copy operation.
            locked_surfaces_.remove(frame_out);
        }

        if (mfx_surface->Data.Locked) {
            // if output is still locked, return it back to locked_surfaces_
            std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
            locked_surfaces_.push_back(frame_out);
        }

        if (work) {
            C2ConstGraphicBlock const_graphic = block->share(rect, C2Fence()/*event.fence()*/);
            C2Buffer out_buffer = MakeC2Buffer( { const_graphic } );

            if (color_aspects_.IsColorAspectsChanged()) {
                out_buffer.setInfo(getColorAspects_l());
                color_aspects_.SignalChangedColorAspectsIsSent();
            }

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
            // Pass end of stream flag only.
            worklet->output.flags = (C2FrameData::flags_t)(work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
            worklet->output.ordinal = work->input.ordinal;

            // Deleter is for keeping source block in lambda capture.
            // block reference count is increased as shared_ptr is captured to the lambda by value.
            auto deleter = [block] (C2Buffer* p) mutable {
                delete p;
                block.reset(); // here block reference count is decreased
            };
            // Make shared_ptr keeping source block during lifetime of output buffer.
            worklet->output.buffers.push_back(
                std::shared_ptr<C2Buffer>(new C2Buffer(out_buffer), deleter));
        }
    } while (false);
    // Release output frame before onWorkDone is called, release causes unmap for system memory.
    frame_out = MfxC2FrameOut();
    if (work) {
        NotifyWorkDone(std::move(work), res);
    }

    {
      std::unique_lock<std::mutex> lock(dev_busy_mutex_);
      --synced_points_count_;
    }
    dev_busy_cond_.notify_one();
}

void MfxC2DecoderComponent::PushPending(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(pending_works_mutex_);

    const auto incoming_frame_timestamp = work->input.ordinal.timestamp;
    auto duplicate = find_if(pending_works_.begin(), pending_works_.end(),
        [incoming_frame_timestamp] (const auto &item) {
            return item.second->input.ordinal.timestamp == incoming_frame_timestamp;
        });
    if (duplicate != pending_works_.end()) {
        MFX_DEBUG_TRACE_STREAM("Potentional error: Found duplicated timestamp: "
                               << duplicate->second->input.ordinal.timestamp.peeku());
    }

    const auto incoming_frame_index = work->input.ordinal.frameIndex;
    auto it = pending_works_.find(incoming_frame_index);
    if (it != pending_works_.end()) { // Shouldn't be the same index there
        NotifyWorkDone(std::move(it->second), C2_CORRUPTED);
        pending_works_.erase(it);
    }
    pending_works_.emplace(incoming_frame_index, std::move(work));
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

            if (eos_received_) { // All works following eos treated as errors.
                item->result = C2_BAD_VALUE;
                // Do this in working thread to follow Drain completion.
                working_queue_.Push( [work = std::move(item), this] () mutable {
                    PushPending(std::move(work));
                });
            } else {
                bool eos = (item->input.flags & C2FrameData::FLAG_END_OF_STREAM);
                bool empty = (item->input.buffers.size() == 0);
                if (eos) {
                    eos_received_ = true;
                }
                if (eos && empty) {
                    working_queue_.Push( [work = std::move(item), this] () mutable {
                        Drain(std::move(work));
                    });
                } else {
                    working_queue_.Push( [ work = std::move(item), this ] () mutable {
                        DoWork(std::move(work));
                    } );
                    if (eos) {
                        working_queue_.Push( [this] () { Drain(nullptr); } );
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

    flushing_ = true;

    working_queue_.Push([this] () {
        MFX_DEBUG_TRACE("DecoderReset");
        MFX_DEBUG_TRACE_STREAM(NAMED(decoder_.get()));
        if (decoder_) { // check if initialized already
            mfxStatus reset_sts = decoder_->Reset(&video_params_);
            MFX_DEBUG_TRACE__mfxStatus(reset_sts);
        }

        if (c2_bitstream_) {
            c2_bitstream_->Reset();
        }

    } );

    // Wait to have no works queued between Queue and DoWork.
    working_queue_.WaitForEmpty();
    waiting_queue_.WaitForEmpty();
    // Turn off flushing mode only after working/waiting queues did all the job,
    // given queue_nb should not be called by libstagefright simultaneously with
    // flush_sm, no threads read/write flushed_works_ list, so it can be used here
    // without block.
    flushing_ = false;

    {
        std::lock_guard<std::mutex> lock(pending_works_mutex_);

        for (auto& item : pending_works_) {
            flushed_works_.push_back(std::move(item.second));
        }
        pending_works_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
        locked_surfaces_.clear();
    }

    *flushedWork = std::move(flushed_works_);
    eos_received_ = false;

    return C2_OK;
}

void MfxC2DecoderComponent::UpdateHdrStaticInfo()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxPayload* pHdrSeiPayload = c2_bitstream_->GetFrameConstructor()->GetSEI(MfxC2HEVCFrameConstructor::SEI_MASTERING_DISPLAY_COLOUR_VOLUME);

    const mfxU32 SEI_MASTERING_DISPLAY_COLOUR_VOLUME_SIZE = 24*8; // required size of data in bits

    if (nullptr != pHdrSeiPayload && pHdrSeiPayload->NumBit >= SEI_MASTERING_DISPLAY_COLOUR_VOLUME_SIZE && nullptr != pHdrSeiPayload->Data)
    {
        MFX_DEBUG_TRACE_MSG("Set HDR static info: SEI_MASTERING_DISPLAY_COLOUR_VOLUME");

        set_hdr_sei_ = true;
        hdr_static_info_.mastering.red.x = pHdrSeiPayload->Data[1] | (pHdrSeiPayload->Data[0] << 8);
        hdr_static_info_.mastering.red.y = pHdrSeiPayload->Data[3] | (pHdrSeiPayload->Data[2] << 8);
        hdr_static_info_.mastering.green.x = pHdrSeiPayload->Data[5] | (pHdrSeiPayload->Data[4] << 8);
        hdr_static_info_.mastering.green.y = pHdrSeiPayload->Data[7] | (pHdrSeiPayload->Data[6] << 8);
        hdr_static_info_.mastering.blue.x = pHdrSeiPayload->Data[9] | (pHdrSeiPayload->Data[8] << 8);
        hdr_static_info_.mastering.blue.y = pHdrSeiPayload->Data[11] | (pHdrSeiPayload->Data[10] << 8);
        hdr_static_info_.mastering.white.x = pHdrSeiPayload->Data[13] | (pHdrSeiPayload->Data[12] << 8);
        hdr_static_info_.mastering.white.y = pHdrSeiPayload->Data[15] | (pHdrSeiPayload->Data[14] << 8);

        mfxU32 mMaxDisplayLuminanceX10000 = pHdrSeiPayload->Data[19] | (pHdrSeiPayload->Data[18] << 8) | (pHdrSeiPayload->Data[17] << 16) | (pHdrSeiPayload->Data[16] << 24);
        hdr_static_info_.mastering.maxLuminance = (mfxU16)(mMaxDisplayLuminanceX10000 / 10000);

        mfxU32 mMinDisplayLuminanceX10000 = pHdrSeiPayload->Data[23] | (pHdrSeiPayload->Data[22] << 8) | (pHdrSeiPayload->Data[21] << 16) | (pHdrSeiPayload->Data[20] << 24);
        hdr_static_info_.mastering.minLuminance = (mfxU16)(mMinDisplayLuminanceX10000 / 10000);
    }
    pHdrSeiPayload = c2_bitstream_->GetFrameConstructor()->GetSEI(MfxC2HEVCFrameConstructor::SEI_CONTENT_LIGHT_LEVEL_INFO);

    const mfxU32 SEI_CONTENT_LIGHT_LEVEL_INFO_SIZE = 4*8; // required size of data in bits

    if (nullptr != pHdrSeiPayload && pHdrSeiPayload->NumBit >= SEI_CONTENT_LIGHT_LEVEL_INFO_SIZE && nullptr != pHdrSeiPayload->Data)
    {
        MFX_DEBUG_TRACE_MSG("Set HDR static info: SEI_CONTENT_LIGHT_LEVEL_INFO");

        hdr_static_info_.maxCll = pHdrSeiPayload->Data[1] | (pHdrSeiPayload->Data[0] << 8);
        hdr_static_info_.maxFall = pHdrSeiPayload->Data[3] | (pHdrSeiPayload->Data[2] << 8);
    }
}

std::shared_ptr<C2StreamColorAspectsInfo::output> MfxC2DecoderComponent::getColorAspects_l(){
    MFX_DEBUG_TRACE_FUNC;
    android::ColorAspects sfAspects;
    std::shared_ptr<C2StreamColorAspectsInfo::output> codedAspects = std::make_shared<C2StreamColorAspectsInfo::output>(0u);

    color_aspects_.GetOutputColorAspects(sfAspects);

    if (!C2Mapper::map(sfAspects.mPrimaries, &codedAspects->primaries)) {
            codedAspects->primaries = C2Color::PRIMARIES_UNSPECIFIED;
    }
    if (!C2Mapper::map(sfAspects.mRange, &codedAspects->range)) {
        codedAspects->range = C2Color::RANGE_UNSPECIFIED;
    }
    if (!C2Mapper::map(sfAspects.mMatrixCoeffs, &codedAspects->matrix)) {
        codedAspects->matrix = C2Color::MATRIX_UNSPECIFIED;
    }
    if (!C2Mapper::map(sfAspects.mTransfer, &codedAspects->transfer)) {
        codedAspects->transfer = C2Color::TRANSFER_UNSPECIFIED;
    }

    MFX_DEBUG_TRACE_I32(codedAspects->primaries);
    MFX_DEBUG_TRACE_I32(codedAspects->range);
    MFX_DEBUG_TRACE_I32(codedAspects->matrix);
    MFX_DEBUG_TRACE_I32(codedAspects->transfer);

    return codedAspects;
}

