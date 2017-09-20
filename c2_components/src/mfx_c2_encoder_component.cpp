/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_encoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_params.h"
#include "mfx_defaults.h"

#include <limits>
#include <thread>
#include <chrono>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_encoder_component"

const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

std::unique_ptr<mfxEncodeCtrl> EncoderControl::AcquireEncodeCtrl()
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<mfxEncodeCtrl> res;

    if (nullptr != ctrl_once_) {
        res = std::move(ctrl_once_);
    } // otherwise return nullptr
    return res;
}

void EncoderControl::Modify(ModifyFunction& function)
{
    MFX_DEBUG_TRACE_FUNC;

    // modify ctrl_once, create if null
    if (nullptr == ctrl_once_) {
        ctrl_once_ = std::make_unique<mfxEncodeCtrl>();
    }
    function(ctrl_once_.get());
}

MfxC2EncoderComponent::MfxC2EncoderComponent(const android::C2String name, int flags, EncoderType encoder_type) :
    MfxC2Component(name, flags),
    encoder_type_(encoder_type),
    synced_points_count_(0)
{
    MFX_DEBUG_TRACE_FUNC;

    switch(encoder_type_) {
        case ENCODER_H264:

            MfxC2ParamReflector& pr = param_reflector_;

            pr.RegisterParam<C2RateControlSetting>("RateControl");
            pr.RegisterParam<C2BitrateTuning>("Bitrate");

            pr.RegisterParam<C2FrameQPSetting>("FrameQP");
            const uint32_t MIN_QP = 1;
            const uint32_t MAX_QP = 51;
            pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_i, MIN_QP, MAX_QP);
            pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_p, MIN_QP, MAX_QP);
            pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_b, MIN_QP, MAX_QP);

            pr.RegisterParam<C2IntraRefreshTuning>("IntraRefresh");
            pr.RegisterSupportedRange<C2IntraRefreshTuning>(&C2IntraRefreshTuning::mValue, (int)false, (int)true);


        break;
    }

    param_reflector_.DumpParams();
}

MfxC2EncoderComponent::~MfxC2EncoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    FreeEncoder();

    session_.Close();
}

void MfxC2EncoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("C2.h264ve",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H264>);
}

android::status_t MfxC2EncoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    Reset();

    mfxStatus mfx_res = MfxDev::Create(&device_);
    if(mfx_res == MFX_ERR_NONE) {
        mfx_res = session_.Init(mfx_implementation_, &g_required_mfx_version);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Major);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Minor);

        if(mfx_res == MFX_ERR_NONE) {
            mfxStatus sts = session_.QueryIMPL(&mfx_implementation_);
            MFX_DEBUG_TRACE__mfxStatus(sts);
            MFX_DEBUG_TRACE_I32(mfx_implementation_);

            mfx_res = device_->InitMfxSession(&session_);
        } else {
            MFX_DEBUG_TRACE_MSG("MFXVideoSession::Init failed");
            MFX_DEBUG_TRACE__mfxStatus(mfx_res);
        }
    }

    return MfxStatusToC2(mfx_res);
}

status_t MfxC2EncoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    synced_points_count_ = 0;

    working_queue_.Start();
    waiting_queue_.Start();

    return C2_OK;
}

status_t MfxC2EncoderComponent::DoStop()
{
    MFX_DEBUG_TRACE_FUNC;

    waiting_queue_.Stop();
    working_queue_.Stop();
    FreeEncoder();

    return C2_OK;
}

mfxStatus MfxC2EncoderComponent::Reset()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;

    switch (encoder_type_)
    {
    case ENCODER_H264:
        video_params_config_.mfx.CodecId = MFX_CODEC_AVC;
        break;
    default:
        MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
        break;
    }

    res = mfx_set_defaults_mfxVideoParam_enc(&video_params_config_);

    // default pattern
    video_params_config_.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;

    return res;
}

mfxStatus MfxC2EncoderComponent::InitEncoder(const mfxFrameInfo& frame_info)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    std::lock_guard<std::mutex> lock(init_encoder_mutex_);

    video_params_config_.mfx.FrameInfo = frame_info;
    {
        encoder_.reset(MFX_NEW_NO_THROW(MFXVideoENCODE(session_)));
        if (nullptr == encoder_) {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
        }

        if (MFX_ERR_NONE == mfx_res) {

            MFX_DEBUG_TRACE_MSG("Encoder initializing...");
            MFX_DEBUG_TRACE__mfxVideoParam_enc(video_params_config_);

            mfx_res = encoder_->Init(&video_params_config_);
            MFX_DEBUG_TRACE_MSG("Encoder initialized");
            MFX_DEBUG_TRACE__mfxStatus(mfx_res);

            if (MFX_WRN_PARTIAL_ACCELERATION == mfx_res) {
                MFX_DEBUG_TRACE_MSG("InitEncoder returns MFX_WRN_PARTIAL_ACCELERATION");
                mfx_res = MFX_ERR_NONE;
            }
        }

        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = encoder_->GetVideoParam(&video_params_state_);
            MFX_DEBUG_TRACE__mfxVideoParam_enc(video_params_state_);
        }

        if (MFX_ERR_NONE != mfx_res) {
            FreeEncoder();
        }
    }
    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

void MfxC2EncoderComponent::FreeEncoder()
{
    MFX_DEBUG_TRACE_FUNC;

    if(nullptr != encoder_) {
        encoder_->Close();
        encoder_ = nullptr;
    }
}

void MfxC2EncoderComponent::RetainLockedFrame(MfxC2FrameIn input)
{
    MFX_DEBUG_TRACE_FUNC;

    if(input.GetMfxFrameSurface()->Data.Locked) {
        locked_frames_.emplace_back(std::move(input));
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
      sts = encoder_->EncodeFrameAsync(ctrl, surface, bs, syncp);
      ++trying_count;

      if (MFX_WRN_DEVICE_BUSY == sts) {

        if (trying_count >= MAX_TRYING_COUNT) {
            MFX_DEBUG_TRACE_MSG("Too many MFX_WRN_DEVICE_BUSY from EncodeFrameAsync");
            sts = MFX_ERR_DEVICE_FAILED;
            break;
        }

        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
        dev_busy_cond_.wait_for(lock, timeout, [this] { return synced_points_count_ < video_params_state_.AsyncDepth; } );
      }
    } while (MFX_WRN_DEVICE_BUSY == sts);

    return sts;
}

status_t MfxC2EncoderComponent::AllocateBitstream(const std::unique_ptr<android::C2Work>& work,
    MfxC2BitstreamOut* mfx_bitstream)
{
    // TODO: allocation pool is required here
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {
        MFX_DEBUG_TRACE_I32(video_params_state_.mfx.BufferSizeInKB);
        MFX_DEBUG_TRACE_I32(video_params_state_.mfx.BRCParamMultiplier);
        mfxU32 required_size = video_params_state_.mfx.BufferSizeInKB * 1000 * video_params_state_.mfx.BRCParamMultiplier;
        MFX_DEBUG_TRACE_I32(required_size);

        if(work->worklets.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple worklets");
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
        C2BufferPack& output = worklet->output;

        if(worklet->allocators.size() != 1 || worklet->output.buffers.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple outputs");
            res = C2_BAD_VALUE;
            break;
        }

        std::shared_ptr<C2BlockAllocator> allocator = worklet->allocators.front();
        C2MemoryUsage mem_usage = { C2MemoryUsage::kSoftwareRead, C2MemoryUsage::kSoftwareWrite };
        std::shared_ptr<C2LinearBlock> out_block;

        res = allocator->allocateLinearBlock(required_size, mem_usage, &out_block);
        if(C2_OK != res) break;

        res = MfxC2BitstreamOut::Create(out_block, TIMEOUT_NS, mfx_bitstream);

    } while(false);

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}

status_t MfxC2EncoderComponent::ApplyWorkTunings(C2Work& work)
{
    status_t res = C2_OK;

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

        // need this temp vector as cannot init vector<smth const> in one step
        std::vector<C2Param*> temp;
        std::transform(worklet->tunings.begin(), worklet->tunings.end(), std::back_inserter(temp),
            [] (const std::unique_ptr<C2Param>& p) { return p.get(); } );

        std::vector<C2Param* const> params(temp.begin(), temp.end());

        std::vector<std::unique_ptr<C2SettingResult>> failures;
        DoConfig(params, &failures, false);
        for(auto& failure : failures) {
            worklet->failures.push_back(std::move(failure));
        }

    } while(false);

    return res;
}

void MfxC2EncoderComponent::DoWork(std::unique_ptr<android::C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    status_t res = C2_OK;

    do {
        C2BufferPack& input = work->input;

        MfxC2FrameIn mfx_frame;
        res = MfxC2FrameIn::Create(input, TIMEOUT_NS, &mfx_frame);
        if(C2_OK != res) break;

        if(nullptr == encoder_) {
            // get frame format and size for encoder init from the first frame
            // should be got from slot descriptor
            mfxStatus mfx_sts = InitEncoder(mfx_frame.GetMfxFrameSurface()->Info);
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

        std::unique_ptr<mfxEncodeCtrl> encode_ctrl = encoder_control_.AcquireEncodeCtrl();

        mfxStatus mfx_sts = EncodeFrameAsync(encode_ctrl.get(),
            mfx_frame.GetMfxFrameSurface(), mfx_bitstream.GetMfxBitstream(), &sync_point);

        if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfx_sts) mfx_sts = MFX_ERR_NONE;

        if( (MFX_ERR_NONE != mfx_sts) && (MFX_ERR_MORE_DATA != mfx_sts) ) {
            MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
            res = MfxStatusToC2(mfx_sts);
            break;
        }

        waiting_queue_.Push( [ input = std::move(mfx_frame), this ] () mutable {
            RetainLockedFrame(std::move(input));
        } );

        pending_works_.push(std::move(work));

        if(MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<android::C2Work> work = std::move(pending_works_.front());

            pending_works_.pop();

            waiting_queue_.Push(
                [ work = std::move(work), ec = std::move(encode_ctrl), bs = std::move(mfx_bitstream), sync_point, this ] () mutable {
                WaitWork(std::move(work), std::move(ec), std::move(bs), sync_point);
            } );

            {
                std::unique_lock<std::mutex> lock(dev_busy_mutex_);
                ++synced_points_count_;
            }
        }

        if(MFX_ERR_MORE_DATA == mfx_sts) mfx_sts = MFX_ERR_NONE;

    } while(false); // fake loop to have a cleanup point there

    if(C2_OK != res) { // notify listener in case of failure only
        NotifyWorkDone(std::move(work), res);
    }
}

void MfxC2EncoderComponent::Drain()
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    while (!pending_works_.empty()) {

        MfxC2BitstreamOut mfx_bitstream;
        res = AllocateBitstream(pending_works_.front(), &mfx_bitstream);
        if(C2_OK != res) break;

        mfxSyncPoint sync_point;

        std::unique_ptr<mfxEncodeCtrl> encode_ctrl = encoder_control_.AcquireEncodeCtrl();

        mfxStatus mfx_sts = EncodeFrameAsync(encode_ctrl.get(),
            nullptr/*input surface*/, mfx_bitstream.GetMfxBitstream(), &sync_point);

        if (MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<android::C2Work> work = std::move(pending_works_.front());

            pending_works_.pop();

            waiting_queue_.Push(
                [ work = std::move(work), ec = std::move(encode_ctrl), bs = std::move(mfx_bitstream), sync_point, this ] () mutable {
                WaitWork(std::move(work), std::move(ec), std::move(bs), sync_point);
            } );

            {
                std::unique_lock<std::mutex> lock(dev_busy_mutex_);
                ++synced_points_count_;
            }
        } else {
            // MFX_ERR_MORE_DATA is an error here too -
            // we are calling EncodeFrameAsync times exactly how many outputs should be fetched
            res = MfxStatusToC2(mfx_sts);
            break;
        }
    }

    if(C2_OK != res) {
        while(!pending_works_.empty()) {
            NotifyWorkDone(std::move(pending_works_.front()), res);
            pending_works_.pop();
        }
    }
}

void MfxC2EncoderComponent::WaitWork(std::unique_ptr<C2Work>&& work,
    std::unique_ptr<mfxEncodeCtrl>&& encode_ctrl,
    MfxC2BitstreamOut&& bit_stream, mfxSyncPoint sync_point)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    mfx_res = session_.SyncOperation(sync_point, MFX_C2_INFINITE);

    if (MFX_ERR_NONE != mfx_res) {
        MFX_DEBUG_TRACE_MSG("SyncOperation failed");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    }

    // checking for unlocked surfaces and releasing them
    locked_frames_.erase(
        std::remove_if(
            locked_frames_.begin(),
            locked_frames_.end(),
            [] (const MfxC2FrameIn& mfx_frame)->bool { return !mfx_frame.GetMfxFrameSurface()->Data.Locked; } ),
        locked_frames_.end());

    // release encode_ctrl
    encode_ctrl = nullptr;

    if(MFX_ERR_NONE == mfx_res) {

        C2Event event;
        event.fire(); // pre-fire event as output buffer is ready to use

        mfxBitstream* mfx_bitstream = bit_stream.GetMfxBitstream();
        MFX_DEBUG_TRACE_P(mfx_bitstream);

        if(!mfx_bitstream) mfx_res = MFX_ERR_NULL_PTR;
        else {
            MFX_DEBUG_TRACE_U32(mfx_bitstream->DataOffset);
            MFX_DEBUG_TRACE_U32(mfx_bitstream->DataLength);

            C2ConstLinearBlock const_linear = bit_stream.GetC2LinearBlock()->share(
                mfx_bitstream->DataOffset,
                mfx_bitstream->DataLength, event.fence());
            C2BufferData out_buffer_data = const_linear;

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

            worklet->output.ordinal.timestamp = work->input.ordinal.timestamp;
            worklet->output.ordinal.frame_index = work->input.ordinal.frame_index;
            worklet->output.ordinal.custom_ordinal = work->input.ordinal.custom_ordinal;

            worklet->output.buffers.front() = std::make_shared<C2Buffer>(out_buffer_data);
        }
    }

    NotifyWorkDone(std::move(work), MfxStatusToC2(mfx_res));

    {
      std::unique_lock<std::mutex> lock(dev_busy_mutex_);
      --synced_points_count_;
    }
    dev_busy_cond_.notify_one();
}

std::unique_ptr<mfxVideoParam> MfxC2EncoderComponent::GetParamsView() const
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<mfxVideoParam> res = std::make_unique<mfxVideoParam>();
    mfxStatus sts = MFX_ERR_NONE;

    if(nullptr == encoder_) {
        mfxVideoParam* in_params = const_cast<mfxVideoParam*>(&video_params_config_);

        res->mfx.CodecId = in_params->mfx.CodecId;

        sts = MFXVideoENCODE_Query(
            (mfxSession)*const_cast<MFXVideoSession*>(&session_),
            in_params, res.get());
    } else {
        sts = encoder_->GetVideoParam(res.get());
    }

    MFX_DEBUG_TRACE__mfxStatus(sts);
    if (MFX_ERR_NONE != sts) {
        res = nullptr;
    } else {
        MFX_DEBUG_TRACE__mfxVideoParam_enc((*res));
    }
    return res;
}

status_t MfxC2EncoderComponent::QueryParam(const mfxVideoParam* src, C2Param::Type type, C2Param** dst) const
{
    status_t res = C2_OK;

    switch (type.paramIndex()) {
        case kParamIndexRateControl: {
            if (nullptr == *dst) {
                *dst = new C2RateControlSetting();
            }
            C2RateControlSetting* rate_control = (C2RateControlSetting*)*dst;
            switch(src->mfx.RateControlMethod) {
                case MFX_RATECONTROL_CBR: rate_control->mValue = C2RateControlCBR; break;
                case MFX_RATECONTROL_VBR: rate_control->mValue = C2RateControlVBR; break;
                case MFX_RATECONTROL_CQP: rate_control->mValue = C2RateControlCQP; break;
                default:
                    res = C2_CORRUPTED;
                    break;
            }
            break;
        }
        case kParamIndexBitrate: {
            if (nullptr == *dst) {
                *dst = new C2BitrateTuning();
            }
            C2BitrateTuning* bitrate = (C2BitrateTuning*)*dst;
            if (src->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                bitrate->mValue = src->mfx.TargetKbps;
            } else {
                res = C2_CORRUPTED;
            }
            break;
        }
        case kParamIndexFrameQP: {
            if (nullptr == *dst) {
                *dst = new C2FrameQPSetting();
            }
            C2FrameQPSetting* frame_qp = (C2FrameQPSetting*)*dst;
            if (src->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
                frame_qp->qp_i = src->mfx.QPI;
                frame_qp->qp_p = src->mfx.QPP;
                frame_qp->qp_b = src->mfx.QPB;
            } else {
                res = C2_CORRUPTED;
            }
            break;
        }

        default:
        res = C2_BAD_INDEX;
        break;
    }
    return res;
}

status_t MfxC2EncoderComponent::query_nb(
    const std::vector<C2Param* const> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(init_encoder_mutex_);

    status_t res = C2_OK;

    // determine source, update it if needed
    std::unique_ptr<mfxVideoParam> params_view = GetParamsView();
    if (nullptr != params_view) {
        // 1st cycle on stack params
        for (C2Param* param : stackParams) {
            status_t param_res = C2_OK;
            if (param_reflector_.FindParam(param->type())) {
                param_res = QueryParam(params_view.get(), param->type(), &param);
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
            status_t param_res = C2_OK;
            if (param_reflector_.FindParam(param_index.type())) {
                param_res = QueryParam(params_view.get(), param_index.type(), &param);
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

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}

void MfxC2EncoderComponent::DoConfig(const std::vector<C2Param* const> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures,
    bool queue_update)
{
    MFX_DEBUG_TRACE_FUNC;

    for (const C2Param* param : params) {
        // check whether plugin supports this parameter
        std::unique_ptr<C2SettingResult> find_res = param_reflector_.FindParam(param);
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
        if(!param_reflector_.ValidateParam(param, failures)) {
            continue;
        }

        // applying parameter
        switch (C2Param::Type(param->type()).paramIndex()) {
            case kParamIndexRateControl: {
                mfxStatus sts = MFX_ERR_NONE;
                switch (static_cast<const C2RateControlSetting*>(param)->mValue) {
                    case C2RateControlCBR:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, &video_params_config_);
                        break;
                    case C2RateControlVBR:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_VBR, &video_params_config_);
                        break;
                    case C2RateControlCQP:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CQP, &video_params_config_);
                        break;
                    default:
                        sts = MFX_ERR_INVALID_VIDEO_PARAM;
                        break;
                }
                if(MFX_ERR_NONE != sts) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexBitrate:
                if (video_params_config_.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                    uint32_t bitrate_value = static_cast<const C2BitrateTuning*>(param)->mValue;
                    if (state_ == State::STOPPED) {
                        video_params_config_.mfx.TargetKbps = bitrate_value;
                    } else if (video_params_config_.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
                        auto update_bitrate_value = [this, bitrate_value] () {
                            MFX_DEBUG_TRACE("update_bitrate_value");
                            MFX_DEBUG_TRACE_I32(bitrate_value);
                            video_params_config_.mfx.TargetKbps = bitrate_value;
                            if (nullptr != encoder_) {
                                {   // waiting for encoding completion of all enqueued frames
                                    std::unique_lock<std::mutex> lock(dev_busy_mutex_);
                                    // set big enough value to not hang if something unexpected happens
                                    const auto timeout = std::chrono::seconds(1);
                                    bool wait_res = dev_busy_cond_.wait_for(lock, timeout, [this] { return synced_points_count_ == 0; } );
                                    if (!wait_res) {
                                        MFX_DEBUG_TRACE_MSG("WRN: Some encoded frames might skip during tunings change.");
                                    }
                                }
                                mfxStatus reset_sts = encoder_->Reset(&video_params_config_);
                                MFX_DEBUG_TRACE__mfxStatus(reset_sts);
                            }
                        };

                        Drain();

                        if (queue_update) {
                            working_queue_.Push(std::move(update_bitrate_value));
                        } else {
                            update_bitrate_value();
                        }
                    } else {
                        // If state is executing and rate control is not VBR, Reset will not update bitrate,
                        // so report an error.
                        failures->push_back(MakeC2SettingResult(C2ParamField(param),
                            C2SettingResult::CONFLICT, { MakeC2ParamField<C2RateControlSetting>() } ));
                    }
                } else {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param),
                        C2SettingResult::CONFLICT, { MakeC2ParamField<C2RateControlSetting>() } ));
                }
                break;
            case kParamIndexFrameQP: {
                const C2FrameQPSetting* qp_setting = static_cast<const C2FrameQPSetting*>(param);
                    if(video_params_config_.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                        failures->push_back(MakeC2SettingResult(C2ParamField(param),
                            C2SettingResult::CONFLICT, { MakeC2ParamField<C2RateControlSetting>() } ));
                    } else {
                        video_params_config_.mfx.QPI = qp_setting->qp_i;
                        video_params_config_.mfx.QPP = qp_setting->qp_p;
                        video_params_config_.mfx.QPB = qp_setting->qp_b;
                    }
                break;
            }
            case kParamIndexIntraRefresh: {
                const C2IntraRefreshTuning* intra_refresh = static_cast<const C2IntraRefreshTuning*>(param);
                if (intra_refresh->mValue != 0) {

                    auto update = [this] () {
                        EncoderControl::ModifyFunction modify = [] (mfxEncodeCtrl* ctrl) {
                            ctrl->FrameType = MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I;
                        };
                        encoder_control_.Modify(modify);
                    };

                    if (queue_update) {
                        working_queue_.Push(std::move(update));
                    } else {
                        update();
                    }
                }
                break;
            }
            default:
                failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
                break;
        }
    }
}

status_t MfxC2EncoderComponent::config_nb(const std::vector<C2Param* const> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures) {

    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {
        if (nullptr == failures) {
            res = C2_CORRUPTED; break;
        }

        failures->clear();

        std::lock(init_encoder_mutex_, state_mutex_);
        std::lock_guard<std::mutex> lock1(init_encoder_mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(state_mutex_, std::adopt_lock);

        DoConfig(params, failures, true);

        res = GetAggregateStatus(failures);

    } while(false);

    return res;
}

status_t MfxC2EncoderComponent::queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& item : *items) {

        bool eos = (item->input.flags & BUFFERFLAG_END_OF_STREAM);

        working_queue_.Push( [ work = std::move(item), this ] () mutable {

            DoWork(std::move(work));

        } );

        if(eos) {
            working_queue_.Push( [this] () { Drain(); } );
        }
    }

    return C2_OK;
}
