/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_encoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_params.h"
#include "mfx_defaults.h"
#include "C2PlatformSupport.h"

#include <limits>
#include <thread>
#include <chrono>
#include <iomanip>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_encoder_component"

const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

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

MfxC2EncoderComponent::MfxC2EncoderComponent(const C2String name, const CreateConfig& config,
    std::shared_ptr<MfxC2ParamReflector> reflector, EncoderType encoder_type) :
        MfxC2Component(name, config, std::move(reflector)),
        encoder_type_(encoder_type),
        synced_points_count_(0)
{
    MFX_DEBUG_TRACE_FUNC;

    switch(encoder_type_) {
        case ENCODER_H264:
        case ENCODER_H265:

            MfxC2ParamStorage& pr = param_storage_;

            pr.RegisterParam<C2RateControlSetting>("RateControl");
            pr.RegisterParam<C2FrameRateSetting::output>("FrameRate");
            pr.RegisterParam<C2BitrateTuning::output>("Bitrate");

            pr.RegisterParam<C2FrameQPSetting>("FrameQP");
            const uint32_t MIN_QP = 1;
            const uint32_t MAX_QP = 51;
            pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_i, MIN_QP, MAX_QP);
            pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_p, MIN_QP, MAX_QP);
            pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_b, MIN_QP, MAX_QP);

            pr.RegisterParam<C2IntraRefreshTuning>("IntraRefresh");
            pr.RegisterSupportedRange<C2IntraRefreshTuning>(&C2IntraRefreshTuning::value, (int)false, (int)true);

            pr.RegisterParam<C2ProfileSetting>("Profile");
            pr.RegisterParam<C2LevelSetting>("Level");
            pr.RegisterParam<C2ProfileLevelInfo::output>("SupportedProfilesLevels");

            pr.RegisterParam<C2MemoryTypeSetting>("MemoryType");

            pr.AddConstValue(C2_PARAMKEY_COMPONENT_DOMAIN,
                std::make_unique<C2ComponentDomainSetting>(C2Component::DOMAIN_VIDEO));

            pr.AddConstValue(C2_PARAMKEY_COMPONENT_KIND,
                std::make_unique<C2ComponentKindSetting>(C2Component::KIND_ENCODER));

            const unsigned int SINGLE_STREAM_ID = 0u;
            pr.AddConstValue(C2_NAME_INPUT_STREAM_FORMAT_SETTING,
                std::make_unique<C2StreamFormatConfig::input>(SINGLE_STREAM_ID, C2FormatVideo));
            pr.AddConstValue(C2_NAME_OUTPUT_STREAM_FORMAT_SETTING,
                std::make_unique<C2StreamFormatConfig::output>(SINGLE_STREAM_ID, C2FormatCompressed));

            pr.AddConstValue(C2_NAME_INPUT_PORT_MIME_SETTING,
                AllocUniqueString<C2PortMimeConfig::input>("video/raw"));
            pr.AddConstValue(C2_NAME_OUTPUT_PORT_MIME_SETTING,
                AllocUniqueString<C2PortMimeConfig::output>("video/avc"));

            pr.AddStreamInfo<C2VideoSizeStreamTuning::input>(
                C2_NAME_STREAM_VIDEO_SIZE_SETTING, SINGLE_STREAM_ID,
                [this] (C2VideoSizeStreamTuning::input* dst)->bool {
                    MFX_DEBUG_TRACE("AssignPictureSize");
                    dst->width = video_params_config_.mfx.FrameInfo.Width;
                    dst->height = video_params_config_.mfx.FrameInfo.Height;
                    MFX_DEBUG_TRACE_STREAM(NAMED(dst->width) << NAMED(dst->height));
                    return true;
                }
            );

        break;
    }

    param_storage_.DumpParams();
}

MfxC2EncoderComponent::~MfxC2EncoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    Release();
}

void MfxC2EncoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("c2.intel.avc.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H264>);
    registry.RegisterMfxC2Component("c2.intel.hevc.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H265>);
}

c2_status_t MfxC2EncoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    Reset();

    mfxStatus mfx_res = MfxDev::Create(MfxDev::Usage::Encoder, &device_);

    if(mfx_res == MFX_ERR_NONE) mfx_res = InitSession();

    return MfxStatusToC2(mfx_res);
}

c2_status_t MfxC2EncoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    synced_points_count_ = 0;
    mfxStatus mfx_res = MFX_ERR_NONE;
    header_sent_ = false;

    do {
        bool allocator_required = (video_params_config_.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY);

        if (allocator_required != allocator_set_) {

            mfx_res = session_.Close();
            if (MFX_ERR_NONE != mfx_res) break;

            mfx_res = InitSession();
            if (MFX_ERR_NONE != mfx_res) break;

            // set frame allocator
            if (allocator_required) {

                std::shared_ptr<MfxFrameAllocator> allocator = device_->GetFrameAllocator();
                if (!allocator) {
                    mfx_res = MFX_ERR_NOT_INITIALIZED;
                    break;
                }

                mfx_res = session_.SetFrameAllocator(&allocator->GetMfxAllocator());
                if (MFX_ERR_NONE != mfx_res) break;

            } else {
                mfx_res = session_.SetFrameAllocator(nullptr);
                if (MFX_ERR_NONE != mfx_res) break;
            }
            allocator_set_ = allocator_required;
        }
        working_queue_.Start();
        waiting_queue_.Start();

        if (create_config_.dump_output) {

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::ostringstream oss;
            std::tm local_tm;
            localtime_r(&now_c, &local_tm);

            oss << name_ << "-" << std::put_time(std::localtime(&now_c), "%Y%m%d%H%M%S") << ".bin";

            MFX_DEBUG_TRACE_STREAM("Encoder output dump is started to " <<
                MFX_C2_DUMP_DIR << "/" << MFX_C2_DUMP_OUTPUT_SUB_DIR << "/" <<
                oss.str());

            output_writer_ = std::make_unique<BinaryWriter>(MFX_C2_DUMP_DIR,
                std::vector<std::string>({MFX_C2_DUMP_OUTPUT_SUB_DIR}), oss.str());
        }

    } while(false);

    return C2_OK;
}

c2_status_t MfxC2EncoderComponent::DoStop(bool abort)
{
    MFX_DEBUG_TRACE_FUNC;

    if (abort) {
        waiting_queue_.Abort();
        working_queue_.Abort();
    } else {
        waiting_queue_.Stop();
        working_queue_.Stop();
    }

    while (!pending_works_.empty()) {
        NotifyWorkDone(std::move(pending_works_.front()), C2_CANCELED);
        pending_works_.pop();
    }

    FreeEncoder();

    output_writer_.reset();

    return C2_OK;
}

c2_status_t MfxC2EncoderComponent::Release()
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

mfxStatus MfxC2EncoderComponent::InitSession()
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

mfxStatus MfxC2EncoderComponent::Reset()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus res = MFX_ERR_NONE;

    switch (encoder_type_)
    {
    case ENCODER_H264:
        video_params_config_.mfx.CodecId = MFX_CODEC_AVC;
        break;
   case ENCODER_H265:
        video_params_config_.mfx.CodecId = MFX_CODEC_HEVC;
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
    if (MFX_ERR_NONE == mfx_res) {
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

void MfxC2EncoderComponent::RetainLockedFrame(MfxC2FrameIn&& input)
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

c2_status_t MfxC2EncoderComponent::AllocateBitstream(const std::unique_ptr<C2Work>& work,
    MfxC2BitstreamOut* mfx_bitstream)
{
    // TODO: allocation pool is required here
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

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

        if(worklet->output.buffers.size() != 0) {
            MFX_DEBUG_TRACE_MSG("Caller is not supposed to allocate output");
            res = C2_BAD_VALUE;
            break;
        }

        C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        std::shared_ptr<C2LinearBlock> out_block;

        res = c2_allocator_->fetchLinearBlock(required_size, mem_usage, &out_block);
        if(C2_OK != res) break;

        res = MfxC2BitstreamOut::Create(out_block, TIMEOUT_NS, mfx_bitstream);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2EncoderComponent::ApplyWorkTunings(C2Work& work)
{
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
            DoConfig(params, &failures, false);
        }
        for(auto& failure : failures) {
            worklet->failures.push_back(std::move(failure));
        }

    } while(false);

    return res;
}

void MfxC2EncoderComponent::DoWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    c2_status_t res = C2_OK;

    do {
        if (!c2_allocator_) {
            res = GetCodec2BlockPool(C2BlockPool::BASIC_LINEAR,
                shared_from_this(), &c2_allocator_);
            if (res != C2_OK) break;
        }

        C2FrameData& input = work->input;

        std::shared_ptr<MfxFrameConverter> frame_converter;
        if (video_params_config_.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY) {
            frame_converter = device_->GetFrameConverter();
        }

        MfxC2FrameIn mfx_frame;

        res = MfxC2FrameIn::Create(frame_converter, input, video_params_config_.mfx.FrameInfo, TIMEOUT_NS, &mfx_frame);
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

        waiting_queue_.Push( [ mfx_frame = std::move(mfx_frame), this ] () mutable {
            RetainLockedFrame(std::move(mfx_frame));
        } );

        pending_works_.push(std::move(work));

        if(MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<C2Work> work = std::move(pending_works_.front());

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

    c2_status_t res = C2_OK;

    while (!pending_works_.empty()) {

        MfxC2BitstreamOut mfx_bitstream;
        res = AllocateBitstream(pending_works_.front(), &mfx_bitstream);
        if(C2_OK != res) break;

        mfxSyncPoint sync_point;

        std::unique_ptr<mfxEncodeCtrl> encode_ctrl = encoder_control_.AcquireEncodeCtrl();

        mfxStatus mfx_sts = EncodeFrameAsync(encode_ctrl.get(),
            nullptr/*input surface*/, mfx_bitstream.GetMfxBitstream(), &sync_point);

        if (MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<C2Work> work = std::move(pending_works_.front());

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

    mfx_res = session_.SyncOperation(sync_point, MFX_TIMEOUT_INFINITE);

    if (MFX_ERR_NONE != mfx_res) {
        MFX_DEBUG_TRACE_MSG("SyncOperation failed");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    }

    // checking for unlocked surfaces and releasing them
    locked_frames_.remove_if(
        [] (const MfxC2FrameIn& mfx_frame)->bool { return !mfx_frame.GetMfxFrameSurface()->Data.Locked; } );

    // release encode_ctrl
    encode_ctrl = nullptr;

    if(MFX_ERR_NONE == mfx_res) {

        C2Event event;
        event.fire(); // pre-fire event as output buffer is ready to use

        mfxBitstream* mfx_bitstream = bit_stream.GetMfxBitstream();
        MFX_DEBUG_TRACE_P(mfx_bitstream);

        if(!mfx_bitstream) mfx_res = MFX_ERR_NULL_PTR;
        else {
            MFX_DEBUG_TRACE_STREAM(NAMED(mfx_bitstream->DataOffset) << NAMED(mfx_bitstream->DataLength));

            if (output_writer_ && mfx_bitstream->DataLength > 0) {
                output_writer_->Write(mfx_bitstream->Data + mfx_bitstream->DataOffset,
                    mfx_bitstream->DataLength);
            }

            C2ConstLinearBlock const_linear = bit_stream.GetC2LinearBlock()->share(
                mfx_bitstream->DataOffset,
                mfx_bitstream->DataLength, event.fence());
            C2Buffer out_buffer = MakeC2Buffer( { const_linear } );
            if ((mfx_bitstream->FrameType & MFX_FRAMETYPE_IDR) != 0) {
                out_buffer.setInfo(std::make_shared<C2StreamPictureTypeMaskInfo::output>(0u/*stream id*/, C2PictureTypeKeyFrame));
            }

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

            worklet->output.flags = work->input.flags;
            if (!header_sent_) {

                mfxExtCodingOptionSPSPPS spspps{};
                mfxVideoParam video_param{};

                mfxExtBuffer* ext_buf = &spspps.Header;

                spspps.Header.BufferId = MFX_EXTBUFF_CODING_OPTION_SPSPPS;
                spspps.Header.BufferSz = sizeof(mfxExtCodingOptionSPSPPS);
                mfxU8 buf[1024] = {0};
                spspps.SPSBuffer = buf;
                spspps.SPSBufSize = 512;
                spspps.PPSBuffer = buf + spspps.SPSBufSize;
                spspps.PPSBufSize = 512;

                video_param.NumExtParam = 1;
                video_param.ExtParam = &ext_buf;

                mfx_res = encoder_->GetVideoParam(&video_param);
                if (MFX_ERR_NONE == mfx_res) {

                    const int header_size = spspps.SPSBufSize + spspps.PPSBufSize;

                    std::unique_ptr<C2StreamCsdInfo::output> csd =
                        C2StreamCsdInfo::output::AllocUnique(header_size, 0u);

                    MFX_DEBUG_TRACE_STREAM("SPS: " << FormatHex(spspps.SPSBuffer, spspps.SPSBufSize));
                    MFX_DEBUG_TRACE_STREAM("PPS: " << FormatHex(spspps.PPSBuffer, spspps.PPSBufSize));

                    uint8_t* dst = csd->m.value;
                    std::copy(spspps.SPSBuffer, spspps.SPSBuffer + spspps.SPSBufSize, dst);
                    dst += spspps.SPSBufSize;

                    std::copy(spspps.PPSBuffer, spspps.PPSBuffer + spspps.PPSBufSize, dst);

                    work->worklets.front()->output.configUpdate.push_back(std::move(csd));

                    worklet->output.flags = (C2FrameData::flags_t)(worklet->output.flags | C2FrameData::FLAG_CODEC_CONFIG);
                    header_sent_ = true;
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

        MFX_DEBUG_TRACE__mfxVideoParam_enc((*in_params));

        sts = MFXVideoENCODE_Query(
            (mfxSession)*const_cast<MFXVideoSession*>(&session_),
            in_params, res.get());

        MFX_DEBUG_TRACE__mfxVideoParam_enc((*res));
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

c2_status_t MfxC2EncoderComponent::QueryParam(const mfxVideoParam* src, C2Param::Index index, C2Param** dst) const
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    res = param_storage_.QueryParam(index, dst);
    if (C2_NOT_FOUND == res) {
        res = C2_OK; // suppress error as second pass to find param

        switch (index.typeIndex()) {
            case kParamIndexRateControl: {
                if (nullptr == *dst) {
                    *dst = new C2RateControlSetting();
                }
                C2RateControlSetting* rate_control = (C2RateControlSetting*)*dst;
                switch(src->mfx.RateControlMethod) {
                    case MFX_RATECONTROL_CBR: rate_control->value = C2RateControlCBR; break;
                    case MFX_RATECONTROL_VBR: rate_control->value = C2RateControlVBR; break;
                    case MFX_RATECONTROL_CQP: rate_control->value = C2RateControlCQP; break;
                    default:
                        res = C2_CORRUPTED;
                        break;
                }
                break;
            }
            case kParamIndexFrameRate: {
                if (nullptr == *dst) {
                    *dst = new C2FrameRateSetting::output();
                }
                C2FrameRateSetting* framerate = (C2FrameRateSetting*)*dst;
                framerate->value = (float)src->mfx.FrameInfo.FrameRateExtN / src->mfx.FrameInfo.FrameRateExtD;
                break;
            }
            case kParamIndexBitrate: {
                if (nullptr == *dst) {
                    *dst = new C2BitrateTuning::output();
                }
                C2BitrateTuning* bitrate = (C2BitrateTuning*)*dst;
                if (src->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                    bitrate->value = src->mfx.TargetKbps;
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
            case kParamIndexProfile: {
                if (nullptr == *dst) {
                    *dst = new C2ProfileSetting();
                }
                C2Config::profile_t profile {};
                bool set_res = false;
                switch (encoder_type_) {
                    case ENCODER_H264:
                        set_res = AvcProfileMfxToAndroid(video_params_config_.mfx.CodecProfile, &profile);
                        break;
                    case ENCODER_H265:
                        set_res = HevcProfileMfxToAndroid(video_params_config_.mfx.CodecProfile, &profile);
                        break;
                    default:
                        break;
                }
                if (set_res) {
                    C2ProfileSetting* setting = static_cast<C2ProfileSetting*>(*dst);
                    setting->value = profile;
                } else {
                    res = C2_CORRUPTED;
                }
                break;
            }
            case kParamIndexLevel: {
                if (nullptr == *dst) {
                    *dst = new C2LevelSetting();
                }
                C2Config::level_t level {};
                bool set_res = false;
                switch (encoder_type_) {
                    case ENCODER_H264:
                        set_res = AvcLevelMfxToAndroid(video_params_config_.mfx.CodecLevel, &level);
                        break;
                    case ENCODER_H265:
                        set_res = HevcLevelMfxToAndroid(video_params_config_.mfx.CodecLevel, &level);
                        break;
                    default:
                        break;
                }
                if (set_res) {
                    C2LevelSetting* setting = static_cast<C2LevelSetting*>(*dst);
                    setting->value = level;
                } else {
                    res = C2_CORRUPTED;
                }
                break;
            }
            case kParamIndexProfileLevel:
                if (nullptr == *dst) {
                    if (encoder_type_ == ENCODER_H264) {
                        std::unique_ptr<C2ProfileLevelInfo::output> info =
                            C2ProfileLevelInfo::output::AllocUnique(g_h264_profile_levels_count);

                        for (size_t i = 0; i < g_h264_profile_levels_count; ++i) {
                            info->m.values[i] = g_h264_profile_levels[i];
                        }
                        *dst = info.release();
                    } else if (encoder_type_ == ENCODER_H265) {
                        std::unique_ptr<C2ProfileLevelInfo::output> info =
                            C2ProfileLevelInfo::output::AllocUnique(g_h265_profile_levels_count);

                        for (size_t i = 0; i < g_h265_profile_levels_count; ++i) {
                            info->m.values[i] = g_h265_profile_levels[i];
                        }
                        *dst = info.release();
                    } else {
                        res = C2_CORRUPTED;
                    }
                } else { // not possible to return flexible params through stack
                    res = C2_NO_MEMORY;
                }
                break;
            case kParamIndexMemoryType: {
                if (nullptr == *dst) {
                    *dst = new C2MemoryTypeSetting();
                }

                C2MemoryTypeSetting* setting = static_cast<C2MemoryTypeSetting*>(*dst);
                if (!MfxIOPatternToC2MemoryType(true, video_params_config_.IOPattern, &setting->value)) res = C2_CORRUPTED;
                break;
            }
            default:
                res = C2_BAD_INDEX;
                break;
        }
    }
    return res;
}

c2_status_t MfxC2EncoderComponent::Query(
    std::unique_lock<std::mutex> state_lock,
    const std::vector<C2Param*> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    (void)state_lock;
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(init_encoder_mutex_);

    c2_status_t res = C2_OK;

    // determine source, update it if needed
    std::unique_ptr<mfxVideoParam> params_view = GetParamsView();
    if (nullptr != params_view) {
        // 1st cycle on stack params
        for (C2Param* param : stackParams) {
            c2_status_t param_res = C2_OK;
            if (param_storage_.FindParam(param->index())) {
                param_res = QueryParam(params_view.get(), param->index(), &param);
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
                param_res = QueryParam(params_view.get(), param_index, &param);
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

void MfxC2EncoderComponent::DoConfig(const std::vector<C2Param*> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures,
    bool queue_update)
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
            case kParamIndexRateControl: {
                mfxStatus sts = MFX_ERR_NONE;
                switch (static_cast<const C2RateControlSetting*>(param)->value) {
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
            case kParamIndexFrameRate: {
                float framerate_value = static_cast<const C2FrameRateSetting*>(param)->value;
                video_params_config_.mfx.FrameInfo.FrameRateExtN = uint64_t(framerate_value * 1000); // keep 3 sign after dot
                video_params_config_.mfx.FrameInfo.FrameRateExtD = 1000;
                break;
            }
            case kParamIndexBitrate:
                if (video_params_config_.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                    uint32_t bitrate_value = static_cast<const C2BitrateTuning*>(param)->value;
                    if (state_ == State::STOPPED) {
                        video_params_config_.mfx.TargetKbps = bitrate_value;
                    } else if (video_params_config_.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
                        auto update_bitrate_value = [this, bitrate_value, queue_update, failures, param] () {
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
                                if (MFX_ERR_NONE != reset_sts) {
                                    if (!queue_update) {
                                        failures->push_back(MakeC2SettingResult(C2ParamField(param),
                                            C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                                    }
                                }
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
                            C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                    }
                } else {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param),
                        C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                }
                break;
            case kParamIndexFrameQP: {
                const C2FrameQPSetting* qp_setting = static_cast<const C2FrameQPSetting*>(param);
                    if(video_params_config_.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                        failures->push_back(MakeC2SettingResult(C2ParamField(param),
                            C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                    } else {
                        MFX_DEBUG_TRACE_STREAM(
                            NAMED(qp_setting->qp_i) << NAMED(qp_setting->qp_p) << NAMED(qp_setting->qp_b));
                        video_params_config_.mfx.QPI = qp_setting->qp_i;
                        video_params_config_.mfx.QPP = qp_setting->qp_p;
                        video_params_config_.mfx.QPB = qp_setting->qp_b;
                    }
                break;
            }
            case kParamIndexIntraRefresh: {
                const C2IntraRefreshTuning* intra_refresh = static_cast<const C2IntraRefreshTuning*>(param);
                if (intra_refresh->value != 0) {

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
            case kParamIndexProfile: {
                const C2ProfileSetting* profile_setting = static_cast<const C2ProfileSetting*>(param);
                C2Config::profile_t profile = static_cast<C2Config::profile_t>(profile_setting->value);
                bool set_res = false;
                switch (encoder_type_) {
                    case ENCODER_H264:
                        set_res = AvcProfileAndroidToMfx(profile, &video_params_config_.mfx.CodecProfile);
                        break;
                    case ENCODER_H265:
                        set_res = HevcProfileAndroidToMfx(profile, &video_params_config_.mfx.CodecProfile);
                        break;
                    default:
                        break;
                }

                if (!set_res) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexLevel: {
                const C2LevelSetting* setting = static_cast<const C2LevelSetting*>(param);
                C2Config::level_t level = static_cast<C2Config::level_t>(setting->value);
                bool set_res = false;
                switch (encoder_type_) {
                    case ENCODER_H264:
                        set_res = AvcLevelAndroidToMfx(level, &video_params_config_.mfx.CodecLevel);
                        break;
                    case ENCODER_H265:
                        set_res = HevcLevelAndroidToMfx(level, &video_params_config_.mfx.CodecLevel);
                        break;
                    default:
                        break;
                }

                if (!set_res) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexMemoryType: {
                const C2MemoryTypeSetting* setting = static_cast<const C2MemoryTypeSetting*>(param);
                bool set_res = C2MemoryTypeToMfxIOPattern(true, setting->value, &video_params_config_.IOPattern);
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

c2_status_t MfxC2EncoderComponent::Config(std::unique_lock<std::mutex> state_lock,
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

        std::lock_guard<std::mutex> lock(init_encoder_mutex_);

        DoConfig(params, failures, true);

        res = GetAggregateStatus(failures);

    } while(false);

    return res;
}

c2_status_t MfxC2EncoderComponent::Queue(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& item : *items) {

        bool eos = (item->input.flags & C2FrameData::FLAG_END_OF_STREAM);

        working_queue_.Push( [ work = std::move(item), this ] () mutable {

            DoWork(std::move(work));

        } );

        if(eos) {
            working_queue_.Push( [this] () { Drain(); } );
        }
    }

    return C2_OK;
}
