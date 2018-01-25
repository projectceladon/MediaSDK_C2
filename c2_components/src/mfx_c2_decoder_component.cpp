/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_decoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_defaults.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_decoder_component"

const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

MfxC2DecoderComponent::MfxC2DecoderComponent(const android::C2String name, int flags, DecoderType decoder_type) :
    MfxC2Component(name, flags),
    decoder_type_(decoder_type),
    initialized_(false),
    synced_points_count_(0)
{
    MFX_DEBUG_TRACE_FUNC;

    switch(decoder_type_) {
        case DECODER_H264:

            MfxC2ParamReflector& pr = param_reflector_;

            pr.RegisterParam<C2MemoryTypeSetting>("MemoryType");
        break;
    }

    param_reflector_.DumpParams();
}

MfxC2DecoderComponent::~MfxC2DecoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    FreeDecoder();

    session_.Close();
}

void MfxC2DecoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("C2.h264vd",
        &MfxC2Component::Factory<MfxC2DecoderComponent, DecoderType>::Create<DECODER_H264>);
}

android::status_t MfxC2DecoderComponent::Init()
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
        default:
            MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
            fc_type = MfxC2FC_None;
            break;
        }
        c2_bitstream_ = std::make_unique<MfxC2BitstreamIn>(fc_type);
    }

    return MfxStatusToC2(mfx_res);
}

status_t MfxC2DecoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    synced_points_count_ = 0;
    mfxStatus mfx_res = MFX_ERR_NONE;

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

status_t MfxC2DecoderComponent::DoStop()
{
    MFX_DEBUG_TRACE_FUNC;

    waiting_queue_.Stop();
    working_queue_.Stop();

    last_work_allocator_ = nullptr;

    FreeDecoder();

    return C2_OK;
}

mfxStatus MfxC2DecoderComponent::InitSession()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = session_.Init(mfx_implementation_, &g_required_mfx_version);
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
         (MFX_CODEC_VP8 == video_params_.mfx.CodecId)))
        asyncDepth = 1;
    else
        asyncDepth = 0;

    MFX_DEBUG_TRACE_I32(asyncDepth);
    return asyncDepth;
}

mfxStatus MfxC2DecoderComponent::InitDecoder(std::shared_ptr<C2BlockAllocator> c2_allocator)
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

status_t MfxC2DecoderComponent::QueryParam(const mfxVideoParam* src, C2Param::Type type, C2Param** dst) const
{
    status_t res = C2_OK;

    switch (type.paramIndex()) {
        case kParamIndexMemoryType: {
            if (nullptr == *dst) {
                *dst = new C2MemoryTypeSetting();
            }

            C2MemoryTypeSetting* setting = static_cast<C2MemoryTypeSetting*>(*dst);
            if (!MfxIOPatternToC2MemoryType(false, src->IOPattern, &setting->mValue)) res = C2_CORRUPTED;
            break;
        }
        default:
            res = C2_BAD_INDEX;
            break;
    }
    return res;
}

status_t MfxC2DecoderComponent::query_nb(
    const std::vector<C2Param* const> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(init_decoder_mutex_);

    status_t res = C2_OK;

    // determine source, update it if needed
    const mfxVideoParam* params_view = &video_params_;
    if (nullptr != params_view) {
        // 1st cycle on stack params
        for (C2Param* param : stackParams) {
            status_t param_res = C2_OK;
            if (param_reflector_.FindParam(param->type())) {
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
            status_t param_res = C2_OK;
            if (param_reflector_.FindParam(param_index.type())) {
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

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}

void MfxC2DecoderComponent::DoConfig(const std::vector<C2Param* const> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures,
    bool /*queue_update*/)
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
            case kParamIndexMemoryType: {
                const C2MemoryTypeSetting* setting = static_cast<const C2MemoryTypeSetting*>(param);
                bool set_res = C2MemoryTypeToMfxIOPattern(false, setting->mValue, &video_params_.IOPattern);
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

status_t MfxC2DecoderComponent::config_nb(const std::vector<C2Param* const> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures) {

    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {
        if (nullptr == failures) {
            res = C2_CORRUPTED; break;
        }

        failures->clear();

        std::lock(init_decoder_mutex_, state_mutex_);
        std::lock_guard<std::mutex> lock1(init_decoder_mutex_, std::adopt_lock);
        std::lock_guard<std::mutex> lock2(state_mutex_, std::adopt_lock);

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

mfxStatus MfxC2DecoderComponent::DecodeFrame(mfxBitstream *bs, MfxC2FrameOut&& frame_work)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_sts = MFX_ERR_NONE;
    mfxFrameSurface1 *surface_work = nullptr, *surface_out = nullptr;

    do {

        surface_work = frame_work.GetMfxFrameSurface().get();
        if (nullptr == surface_work) {
            mfx_sts = MFX_ERR_NULL_PTR;
            break;
        }

        MFX_DEBUG_TRACE_P(surface_work);

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

            // add output to waiting pool in case of Decode success only
            locked_surfaces_.push_back(std::move(frame_work));

            MFX_DEBUG_TRACE_P(surface_out);
            if (nullptr != surface_out) {

                MFX_DEBUG_TRACE_I32(surface_out->Data.Locked);
                MFX_DEBUG_TRACE_I64(surface_out->Data.TimeStamp);

                if (MFX_ERR_NONE == mfx_sts) {

                    auto pred_match_surface =
                        [surface_out] (const auto& item) { return item.GetMfxFrameSurface().get() == surface_out; };

                    MfxC2FrameOut frame_out = Extract(locked_surfaces_, pred_match_surface);
                    C2WorkOutput work_output;
                    work_output.frame_ = std::move(frame_out);
                    work_output.work_ = std::move(works_queue_.front());
                    works_queue_.pop();

                    waiting_queue_.Push(
                        [ frame = std::move(work_output), sync_point, this ] () mutable {
                        WaitWork(std::move(frame), sync_point);
                    } );
                    {
                        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
                        ++synced_points_count_;
                    }
                }
            }

            // This happens when we do drain and no more output can be produced,
            // have to return error to upper level to stop drain.
            bool drain_completed = (nullptr == bs && MFX_ERR_MORE_DATA == mfx_sts);
            if (!drain_completed) mfx_sts = MFX_ERR_NONE;

        } else if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mfx_sts) {
            MFX_DEBUG_TRACE_MSG("MFX_ERR_INCOMPATIBLE_VIDEO_PARAM: resolution was changed");
        }
    } while(false); // fake loop to have a cleanup point there

    MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
    return mfx_sts;
}

status_t MfxC2DecoderComponent::AllocateC2Block(std::shared_ptr<C2GraphicBlock>* out_block)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {

        if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
            std::shared_ptr<C2GraphicBlock> block = allocator_->Alloc();
            if (!block) {
                res = C2_TIMED_OUT;
                break;
            }

            C2Rect required_rect(video_params_.mfx.FrameInfo.Width, video_params_.mfx.FrameInfo.Height);
            if (block->width() > required_rect.mWidth || block->height() > required_rect.mHeight) {
                MFX_DEBUG_TRACE_STREAM(NAMED(required_rect.mWidth) << NAMED(required_rect.mHeight));
                block->setCrop(required_rect);
            }

            *out_block = std::move(block);

        } else {

            if (!last_work_allocator_) {
                res = C2_NOT_FOUND;
                break;
            }

            C2MemoryUsage mem_usage = { C2MemoryUsage::kSoftwareRead, C2MemoryUsage::kSoftwareWrite };

            res = last_work_allocator_->allocateGraphicBlock(
                video_params_.mfx.FrameInfo.Width, video_params_.mfx.FrameInfo.Height,
                0/*format*/, mem_usage, out_block);
        }
    } while (false);

    return res;
}

status_t MfxC2DecoderComponent::AllocateFrame(MfxC2FrameOut* frame_out)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    MfxFrameConverter* converter = nullptr;
    if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
        converter = device_->GetFrameConverter();
    }

    do {
        auto pred_unlocked = [] (const MfxC2FrameOut& item) { return !item.GetMfxFrameSurface()->Data.Locked; };

        locked_surfaces_.remove_if(pred_unlocked);

        std::shared_ptr<C2GraphicBlock> out_block;
        res = AllocateC2Block(&out_block);
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
            res = MfxC2FrameOut::Create(converter, out_block, TIMEOUT_NS, frame_out);
            if (C2_OK != res) break;

            // put to map
            if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {
                surfaces_.emplace(out_block->handle(), frame_out->GetMfxFrameSurface());
            }
        } else {
            *frame_out = MfxC2FrameOut(std::move(out_block), it->second);
        }
    } while (C2_TIMED_OUT == res);

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}

void MfxC2DecoderComponent::DoWork(std::unique_ptr<android::C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    std::shared_ptr<C2BlockAllocator> work_allocator = work->worklets.front()->allocators.front();
    if (work_allocator) {
        last_work_allocator_ = work_allocator;
    }

    do {
        res = c2_bitstream_->LoadC2BufferPack(work->input, TIMEOUT_NS);
        if (C2_OK != res) break;

        works_queue_.push(std::move(work));

        // loop repeats DecodeFrame on the same frame
        // if DecodeFrame returns error which is repairable, like resolution change
        bool resolution_change = false;
        do {
            if (!initialized_) {
                mfx_sts = InitDecoder(work_allocator);
                if(MFX_ERR_NONE != mfx_sts) {
                    MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                    res = MfxStatusToC2(mfx_sts);
                    break;
                }
                if (!initialized_) {
                    MFX_DEBUG_TRACE_MSG("Cannot initialize mfx decoder");
                    res = C2_BAD_VALUE;
                    break;
                }
            }

            MfxC2FrameOut frame_out;
            res = AllocateFrame(&frame_out);
            if (C2_OK != res) break;

            mfx_sts = DecodeFrame(c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get(),
                std::move(frame_out));

            resolution_change = (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mfx_sts);
            if (resolution_change) {

                frame_out = MfxC2FrameOut(); // release the frame to be used in Drain

                Drain();

                bool resolution_change_done = false;

                mfx_sts = decoder_->DecodeHeader(c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get(), &video_params_);
                if (MFX_ERR_NONE == mfx_sts) {
                    if (video_params_.mfx.FrameInfo.Width <= max_width_ &&
                        video_params_.mfx.FrameInfo.Height <= max_height_) {

                        mfxStatus reset_res = decoder_->Reset(&video_params_);
                        MFX_DEBUG_TRACE__mfxStatus(reset_res);
                        if (MFX_ERR_NONE == reset_res) {
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
            res = MfxStatusToC2(mfx_sts);
            break;
        }

        mfx_sts = c2_bitstream_->GetFrameConstructor()->Unload();
        if (MFX_ERR_NONE != mfx_sts) {
            MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
            res = MfxStatusToC2(mfx_sts);
            break;
        }

    } while(false); // fake loop to have a cleanup point there

    if (C2_OK != res && nullptr != work) { // notify listener in case of failure only
        NotifyWorkDone(std::move(work), res);
    }
}

void MfxC2DecoderComponent::Drain()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_sts = MFX_ERR_NONE;

    do {

        MfxC2FrameOut frame_out;
        status_t c2_sts = AllocateFrame(&frame_out);
        if (C2_OK != c2_sts) break; // no output allocated, no sense in calling DecodeFrame

        mfx_sts = DecodeFrame(nullptr, std::move(frame_out));
    } while (MFX_ERR_NONE == mfx_sts);
}

void MfxC2DecoderComponent::WaitWork(C2WorkOutput&& work_output, mfxSyncPoint sync_point)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    mfx_res = session_.SyncOperation(sync_point, MFX_C2_INFINITE);

    if (MFX_ERR_NONE != mfx_res) {
        MFX_DEBUG_TRACE_MSG("SyncOperation failed");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    }

    std::unique_ptr<android::C2Work> work = std::move(work_output.work_);
    if (work) {
        if(MFX_ERR_NONE == mfx_res) {

            C2Event event;
            event.fire(); // pre-fire event as output buffer is ready to use

            std::shared_ptr<mfxFrameSurface1> mfx_surface = work_output.frame_.GetMfxFrameSurface();
            MFX_DEBUG_TRACE_P(mfx_surface.get());

            if(!mfx_surface) mfx_res = MFX_ERR_NULL_PTR;
            else {
                MFX_DEBUG_TRACE_I32(mfx_surface->Data.Locked);
                MFX_DEBUG_TRACE_I64(mfx_surface->Data.TimeStamp);

                const C2Rect rect(mfx_surface->Info.CropW, mfx_surface->Info.CropH,
                                mfx_surface->Info.CropX, mfx_surface->Info.CropY);
                C2Rect crop = work_output.frame_.GetC2GraphicBlock()->crop();

                C2ConstGraphicBlock const_graphic = work_output.frame_.GetC2GraphicBlock()->share(work_output.frame_.GetC2GraphicBlock()->crop(), event.fence());
                C2BufferData out_buffer_data = const_graphic;

                std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

                worklet->output.ordinal.timestamp = work->input.ordinal.timestamp;
                worklet->output.ordinal.frame_index = work->input.ordinal.frame_index;
                worklet->output.ordinal.custom_ordinal = work->input.ordinal.custom_ordinal;

                worklet->output.buffers.front() = std::make_shared<C2Buffer>(out_buffer_data);
            }
        }
        NotifyWorkDone(std::move(work), MfxStatusToC2(mfx_res));
    }

    {
      std::unique_lock<std::mutex> lock(dev_busy_mutex_);
      --synced_points_count_;
    }
    dev_busy_cond_.notify_one();
}

status_t MfxC2DecoderComponent::ValidateWork(const std::unique_ptr<android::C2Work>& work)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {

        if(work->worklets.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple worklets");
            res = C2_BAD_VALUE;
            break;
        }

        const std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
        C2BufferPack& output = worklet->output;

        if(worklet->allocators.size() != 1 || worklet->output.buffers.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple outputs");
            res = C2_BAD_VALUE;
            break;
        }
    }
    while (false);

    return res;
}

status_t MfxC2DecoderComponent::queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& item : *items) {

        bool eos = (item->input.flags & BUFFERFLAG_END_OF_STREAM);

        status_t sts = ValidateWork(item);

        if (C2_OK == sts) {
            working_queue_.Push( [ work = std::move(item), this ] () mutable {
                DoWork(std::move(work));
            } );
        } else {
            NotifyWorkDone(std::move(item), sts);
        }

        if(eos) {
            working_queue_.Push( [this] () { Drain(); } );
        }
    }

    return C2_OK;
}
