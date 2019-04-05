/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_decoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"
#include "C2PlatformSupport.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_decoder_component"

const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

MfxC2DecoderComponent::MfxC2DecoderComponent(const C2String name, int flags, DecoderType decoder_type) :
    MfxC2Component(name, flags),
    decoder_type_(decoder_type),
    initialized_(false),
    synced_points_count_(0)
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2ParamStorage& pr = param_storage_;

    pr.RegisterParam<C2MemoryTypeSetting>("MemoryType");

    pr.AddConstValue(C2_PARAMKEY_COMPONENT_DOMAIN,
        std::make_unique<C2ComponentDomainSetting>(C2Component::DOMAIN_VIDEO));

    pr.AddConstValue(C2_PARAMKEY_COMPONENT_KIND,
        std::make_unique<C2ComponentKindSetting>(C2Component::KIND_DECODER));

    const unsigned int SINGLE_STREAM_ID = 0u;
    pr.AddConstValue(C2_NAME_INPUT_STREAM_FORMAT_SETTING,
        std::make_unique<C2StreamFormatConfig::input>(SINGLE_STREAM_ID, C2FormatCompressed));
    pr.AddConstValue(C2_NAME_OUTPUT_STREAM_FORMAT_SETTING,
        std::make_unique<C2StreamFormatConfig::output>(SINGLE_STREAM_ID, C2FormatVideo));

    pr.AddStreamInfo<C2StreamPictureSizeInfo::output>(
        C2_PARAMKEY_PICTURE_SIZE, SINGLE_STREAM_ID,
        [this] (C2StreamPictureSizeInfo::output* dst)->bool {
            MFX_DEBUG_TRACE("AssignPictureSize");
            dst->width = video_params_.mfx.FrameInfo.Width;
            dst->height = video_params_.mfx.FrameInfo.Height;
            MFX_DEBUG_TRACE_STREAM(NAMED(dst->width) << NAMED(dst->height));
            return true;
        }
    );

    pr.AddStreamInfo<C2StreamCropRectInfo::output>(
        C2_PARAMKEY_CROP_RECT, SINGLE_STREAM_ID,
        [this] (C2StreamCropRectInfo::output* dst)->bool {
            MFX_DEBUG_TRACE("AssignCrop");
            dst->width = video_params_.mfx.FrameInfo.CropW;
            dst->height = video_params_.mfx.FrameInfo.CropH;
            dst->left = video_params_.mfx.FrameInfo.CropX;
            dst->top = video_params_.mfx.FrameInfo.CropY;
            MFX_DEBUG_TRACE_STREAM(NAMED(dst->left) << NAMED(dst->top) <<
                NAMED(dst->width) << NAMED(dst->height));
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
            break;
        }
        default:
            break;
    }

    pr.RegisterSupportedValues<C2StreamProfileLevelInfo>(&C2StreamProfileLevelInfo::C2ProfileLevelStruct::profile, supported_profiles);
    pr.RegisterSupportedValues<C2StreamProfileLevelInfo>(&C2StreamProfileLevelInfo::C2ProfileLevelStruct::level, supported_levels);

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

    registry.RegisterMfxC2Component("C2.h264vd",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_H264>);

    registry.RegisterMfxC2Component("C2.h265vd",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_H265>);

    registry.RegisterMfxC2Component("C2.vp9vd",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_VP9>);
}

c2_status_t MfxC2DecoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    Reset();

    mfxStatus mfx_res = MfxDev::Create(MfxDev::Usage::Decoder, &device_);
    if(mfx_res == MFX_ERR_NONE) {
        mfx_res = InitSession();
    }
    if(MFX_ERR_NONE == mfx_res) {
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
        default:
            MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
            fc_type = MfxC2FC_None;
            break;
        }
        c2_bitstream_ = std::make_unique<MfxC2BitstreamIn>(fc_type);
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

    if (abort) {
        waiting_queue_.Abort();
        working_queue_.Abort();
    } else {
        waiting_queue_.Stop();
        working_queue_.Stop();
    }

    {
        std::lock_guard<std::mutex> lock(pending_works_mutex_);
        for (auto& pair : pending_works_) {
            NotifyWorkDone(std::move(pair.second), C2_CANCELED);
        }
        pending_works_.clear();
    }

    c2_allocator_ = nullptr;

    FreeDecoder();

    if (c2_bitstream_) {
        c2_bitstream_->Reset();
    }

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

mfxStatus MfxC2DecoderComponent::Reset()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;

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
    default:
        MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
        break;
    }

    mfx_set_defaults_mfxVideoParam_dec(&video_params_);
    video_params_.IOPattern = MFX_IOPATTERN_OUT_SYSTEM_MEMORY;

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
         (MFX_CODEC_VP8 == video_params_.mfx.CodecId)))
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
        MFX_DEBUG_TRACE("InitDecoder: DecodeHeader");

        if (nullptr == decoder_) {
            decoder_.reset(MFX_NEW_NO_THROW(MFXVideoDECODE(session_)));
            if (nullptr == decoder_) {
                mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        }

        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = decoder_->DecodeHeader(c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get(), &video_params_);
        }
        if (MFX_ERR_NULL_PTR == mfx_res) {
            mfx_res = MFX_ERR_MORE_DATA;
        }
        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = c2_bitstream_->GetFrameConstructor()->Init(video_params_.mfx.CodecProfile, video_params_.mfx.FrameInfo);
        }
    }
    if (MFX_ERR_NONE == mfx_res) {
        MFX_DEBUG_TRACE("InitDecoder: GetAsyncDepth");

        video_params_.AsyncDepth = GetAsyncDepth();
    }
    if (MFX_ERR_NONE == mfx_res) {
        MFX_DEBUG_TRACE("InitDecoder: Init");

        MFX_DEBUG_TRACE__mfxVideoParam_dec(video_params_);

        if (allocator_) allocator_->SetC2Allocator(c2_allocator);

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

c2_status_t MfxC2DecoderComponent::QueryParam(const mfxVideoParam* src, C2Param::Type type, C2Param** dst) const
{
    c2_status_t res = C2_OK;

    res = param_storage_.QueryParam(type, dst);
    if (C2_NOT_FOUND == res) {
        switch (type.typeIndex()) {
            case kParamIndexMemoryType: {
                if (nullptr == *dst) {
                    *dst = new C2MemoryTypeSetting();
                }

                C2MemoryTypeSetting* setting = static_cast<C2MemoryTypeSetting*>(*dst);
                if (!MfxIOPatternToC2MemoryType(false, src->IOPattern, &setting->value)) res = C2_CORRUPTED;
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
                param_res = QueryParam(params_view, param->type(), &param);
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
                param_res = QueryParam(params_view, param_index.type(), &param);
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
            failures->push_back(std::move(find_res));
            continue;
        }
        // check whether plugin is in a correct state to apply this parameter
        bool modifiable = (param->kind() == C2Param::TUNING) ||
            (param->kind() == C2Param::SETTING && state_ == State::STOPPED);

        if (!modifiable) {
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
            default:
                failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
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

    do {
      mfx_res = decoder_->DecodeFrameAsync(bs, surface_work, surface_out, syncp);
      ++trying_count;

      if (MFX_WRN_DEVICE_BUSY == mfx_res) {

        if (trying_count >= MAX_TRYING_COUNT) {
            MFX_DEBUG_TRACE_MSG("Too many MFX_WRN_DEVICE_BUSY from DecodeFrameAsync");
            mfx_res = MFX_ERR_DEVICE_FAILED;
            break;
        }

        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
        dev_busy_cond_.wait_for(lock, timeout, [this] { return synced_points_count_ < video_params_.AsyncDepth; } );
      }
    } while (MFX_WRN_DEVICE_BUSY == mfx_res);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

// Can return MFX_ERR_NONE, MFX_ERR_MORE_DATA, MFX_ERR_MORE_SURFACE in case of success
mfxStatus MfxC2DecoderComponent::DecodeFrame(mfxBitstream *bs, MfxC2FrameOut&& frame_work, bool* expect_output)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_sts = MFX_ERR_NONE;
    mfxFrameSurface1 *surface_work = nullptr, *surface_out = nullptr;
    *expect_output = false;

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

    MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
    return mfx_sts;
}

c2_status_t MfxC2DecoderComponent::AllocateC2Block(uint32_t width, uint32_t height, std::shared_ptr<C2GraphicBlock>* out_block)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {

        if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
            std::shared_ptr<C2GraphicBlock> block = allocator_->Alloc();
            if (!block) {
                res = C2_TIMED_OUT;
                break;
            }

            *out_block = std::move(block);

        } else {

            if (!c2_allocator_) {
                res = C2_NOT_FOUND;
                break;
            }

            C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };

            res = c2_allocator_->fetchGraphicBlock(width, height,
                HAL_PIXEL_FORMAT_NV12_TILED_INTEL, mem_usage, out_block);
        }
    } while (false);

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

    do {
        auto pred_unlocked = [] (const MfxC2FrameOut& item) { return !item.GetMfxFrameSurface()->Data.Locked; };
        {
            std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
            locked_surfaces_.remove_if(pred_unlocked);
        }

        std::shared_ptr<C2GraphicBlock> out_block;
        res = AllocateC2Block(video_params_.mfx.FrameInfo.Width, video_params_.mfx.FrameInfo.Height, &out_block);
        if (C2_TIMED_OUT == res) {
            continue;
        }

        if (C2_OK != res) break;

        auto it = surfaces_.end();
        if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
            it = surfaces_.find(out_block->handle());
        }
        if (it == surfaces_.end()) {
            // haven't been used for decoding yet
            res = MfxC2FrameOut::Create(converter, out_block, video_params_.mfx.FrameInfo, TIMEOUT_NS, frame_out);
            if (C2_OK != res) break;

            // put to map
            if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
                surfaces_.emplace(out_block->handle(), frame_out->GetMfxFrameSurface());
            }
        } else {
            *frame_out = MfxC2FrameOut(std::move(out_block), it->second);
        }
    } while (C2_TIMED_OUT == res);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

void MfxC2DecoderComponent::DoWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    const auto incoming_frame_index = work->input.ordinal.frameIndex;
    const auto incoming_flags = work->input.flags;

    MFX_DEBUG_TRACE_STREAM("work: " << work.get() << "; index: " << incoming_frame_index.peeku() <<
        " flags: " << std::hex << incoming_flags);

    bool expect_output = false;

    do {
        std::unique_ptr<MfxC2BitstreamIn::FrameView> bitstream_view;
        res = c2_bitstream_->AppendFrame(work->input, TIMEOUT_NS, &bitstream_view);
        if (C2_OK != res) break;

        if (work->input.buffers.size() == 0) break;

        PushPending(std::move(work));

        if (!c2_allocator_) {
            res = GetCodec2BlockPool(C2BlockPool::BASIC_GRAPHIC,
                shared_from_this(), &c2_allocator_);
            if (res != C2_OK) break;
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
            }

            mfxBitstream *bs = c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get();
            MfxC2FrameOut frame_out;
            do {
                res = AllocateFrame(&frame_out);
                if (C2_OK != res) break;

                mfx_sts = DecodeFrame(bs, std::move(frame_out), &expect_output);
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
                if (MFX_ERR_NONE == mfx_sts) {
                    if (video_params_.mfx.FrameInfo.Width <= max_width_ &&
                        video_params_.mfx.FrameInfo.Height <= max_height_) {

                        mfxStatus reset_sts = decoder_->Reset(&video_params_);
                        MFX_DEBUG_TRACE__mfxStatus(reset_sts);
                        if (MFX_ERR_NONE == reset_sts) {
                            resolution_change_done = true;
                        }
                    }
                }

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
    if (C2_OK != res || !expect_output || incomplete_frame) {
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
            NotifyWorkDone(std::move(work), res);
        }
    }
}

void MfxC2DecoderComponent::Drain(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_sts = MFX_ERR_NONE;

    if (initialized_) {
        do {

            MfxC2FrameOut frame_out;
            c2_status_t c2_sts = AllocateFrame(&frame_out);
            if (C2_OK != c2_sts) break; // no output allocated, no sense in calling DecodeFrame

            bool expect_output{};
            mfx_sts = DecodeFrame(nullptr, std::move(frame_out), &expect_output);

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

    decltype(C2WorkOrdinalStruct::frameIndex) ready_frame_index{mfx_surface->Data.TimeStamp};

    std::unique_ptr<C2Work> work;

    {
        std::lock_guard<std::mutex> lock(pending_works_mutex_);
        auto it = pending_works_.find(ready_frame_index);
        if (it != pending_works_.end()) {
            work = std::move(it->second);
            pending_works_.erase(it);
        }
    }

    do {

        C2Event event;
        event.fire(); // pre-fire event as output buffer is ready to use

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

        if (video_params_.IOPattern != MFX_IOPATTERN_OUT_VIDEO_MEMORY) {

            {
                std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
                // If output is not Locked - we have to release it asap to not interfere with possible
                // frame mapping in onWorkDone callback,
                // if output is Locked - remove it temporarily from locked_surfaces_ to avoid holding
                // locked_surfaces_mutex_ while long copy operation.
                locked_surfaces_.remove(frame_out);
            }

            if (work && mfx_surface->Data.Locked) {
                // if output is locked, we have to copy its contents for output
                std::shared_ptr<C2GraphicBlock> new_block;
                res = AllocateC2Block(block->width(), block->height(), &new_block);
                if (C2_OK != res) break;

                std::unique_ptr<C2GraphicView> new_view;

                res = MapGraphicBlock(*new_block, TIMEOUT_NS, &new_view);
                if (C2_OK != res) break;

                res = CopyGraphicView(frame_out.GetC2GraphicView().get(), new_view.get());
                if (C2_OK != res) break;

                block = new_block; // use new_block to deliver decoded result
            }

            if (mfx_surface->Data.Locked) {
                // if output is still locked, return it back to locked_surfaces_
                std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
                locked_surfaces_.push_back(frame_out);
            }
        }

        if (work) {
            C2ConstGraphicBlock const_graphic = block->share(rect, event.fence());
            C2Buffer out_buffer = MakeC2Buffer( { const_graphic } );

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
    std::lock_guard<std::mutex> lock(pending_works_mutex_);
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

    //TODO: check if working_queue_ might be flushed too, (queued works which doesn't reach DoWork yet)
    working_queue_.Push([this] () {
        MFX_DEBUG_TRACE("DecoderReset");
        mfxStatus reset_sts = decoder_->Reset(&video_params_);
        MFX_DEBUG_TRACE__mfxStatus(reset_sts);
    } );
    // Wait to have no works queued between Queue and DoWork.
    working_queue_.WaitForEmpty();

    {
        std::lock_guard<std::mutex> lock(pending_works_mutex_);

        for (auto& item : pending_works_) {
            flushedWork->push_back(std::move(item.second));
        }
        pending_works_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(locked_surfaces_mutex_);
        locked_surfaces_.clear();
    }
    return C2_OK;
}
