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

        MFX_DEBUG_TRACE_STREAM(surfaces_.Size());

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
    video_params_.IOPattern = MFX_IOPATTERN_OUT_VIDEO_MEMORY;

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
        MFX_DEBUG_TRACE("InitDecoder: QueryIOSurf");

        video_params_.AsyncDepth = GetAsyncDepth();

        mfxFrameAllocRequest request;
        MFX_ZERO_MEMORY(request);

        mfx_res = decoder_->QueryIOSurf(&video_params_, &request);
        if (MFX_WRN_PARTIAL_ACCELERATION == mfx_res) {
            MFX_DEBUG_TRACE_MSG("[WRN] MFX_WRN_PARTIAL_ACCELERATION was received.");
            mfx_res = MFX_ERR_NONE;
        }
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

    if(nullptr != decoder_) {
        decoder_->Close();
        decoder_ = nullptr;
    }

    std::unique_ptr<MfxC2FrameOut> mfx_frame = surfaces_.AcquireUnlockedFrame();
    while(nullptr != mfx_frame) {
        NotifyWorkDone(mfx_frame->GetC2Work(), C2_OK);
        mfx_frame = surfaces_.AcquireUnlockedFrame();
    }

    if (allocator_) {
        allocator_->Reset();
    }
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

status_t MfxC2DecoderComponent::DecodeFrame(mfxBitstream *bs, std::unique_ptr<MfxC2FrameOut>&& mfx_frame)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;
    mfxFrameSurface1 *surface_work = nullptr, *surface_out = nullptr;

    do {
        if (nullptr == mfx_frame) {
            res = C2_BAD_VALUE;
            break;
        }

        surface_work = mfx_frame->GetMfxFrameSurface();
        if (nullptr == surface_work) {
            res = C2_BAD_VALUE;
            break;
        }

        MFX_DEBUG_TRACE_P(mfx_frame.get());
        MFX_DEBUG_TRACE_P(surface_work);

        surfaces_.AddFrame(std::move(mfx_frame));

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
            MFX_DEBUG_TRACE_P(surface_out);
            if (nullptr != surface_out) {

                MFX_DEBUG_TRACE_I32(surface_out->Data.Locked);
                MFX_DEBUG_TRACE_I64(surface_out->Data.TimeStamp);

                if (MFX_ERR_NONE == mfx_sts) {

                    waiting_queue_.Push(
                        [ frame = surfaces_.AcquireFrameBySurface(surface_out), sync_point, this ] () mutable {
                        WaitWork(std::move(frame), sync_point);
                    } );

                    {
                        std::unique_lock<std::mutex> lock(dev_busy_mutex_);
                        ++synced_points_count_;
                    }
                }
            } else if (nullptr == bs && MFX_ERR_MORE_DATA == mfx_sts) {
                // This happens when we do drain and no more output can be produced,
                // have to return error to upper level to stop drain.
                res = MfxStatusToC2(mfx_sts);
            }
        } else if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mfx_sts) {
            MFX_DEBUG_TRACE_MSG("MFX_ERR_INCOMPATIBLE_VIDEO_PARAM: resolution was changed");
            res = C2_BAD_VALUE;
            break;

        } else {
            res = MfxStatusToC2(mfx_sts);
            break;
        }

    } while(false); // fake loop to have a cleanup point there

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}

status_t MfxC2DecoderComponent::AllocateFrame(const std::unique_ptr<android::C2Work>& work,
    std::unique_ptr<MfxC2FrameOut>& mfx_frame)
{
    // TODO: allocation pool is required here
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {

        if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY || work == nullptr) {
            // The condition above is true when we should not use C2 allocator from the C2Work:
            // 1) If video memory is used and all frames are pre-allocated already
            // 2) If system memory is used and we drain, no work for it
            // and surfaces_ should have remaining frames.
            mfx_frame = surfaces_.AcquireUnlockedFrame();
        }

        if (nullptr == mfx_frame) {
            MfxFrameConverter* frame_converter = nullptr;
            std::shared_ptr<C2GraphicBlock> out_block;

            if (video_params_.IOPattern == MFX_IOPATTERN_OUT_VIDEO_MEMORY) {

                out_block = allocator_->Alloc();
                MFX_DEBUG_TRACE_P(out_block.get());
                frame_converter = device_->GetFrameConverter();
            } else {

                if (nullptr == work) {
                    MFX_DEBUG_TRACE_MSG("No work supplied for system memory allocation");
                    res = C2_BAD_VALUE;
                    break;
                }

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

                res = allocator->allocateGraphicBlock(video_params_.mfx.FrameInfo.Width, video_params_.mfx.FrameInfo.Height, 0/*format*/, mem_usage, &out_block);
                if(C2_OK != res) break;
            }

            mfx_frame = std::make_unique<MfxC2FrameOut>();
            MFX_DEBUG_TRACE_P(out_block.get());
            res = MfxC2FrameOut::Create(frame_converter, out_block, TIMEOUT_NS, mfx_frame);
        }
    } while(false);

    MFX_DEBUG_TRACE__android_status_t(res);
    return res;
}

void MfxC2DecoderComponent::DoWork(std::unique_ptr<android::C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    do {
        res = c2_bitstream_->LoadC2BufferPack(work->input, TIMEOUT_NS);
        if (C2_OK != res) break;

        if (!initialized_) {
            mfx_sts = InitDecoder(work->worklets.front()->allocators.front());
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

        std::unique_ptr<MfxC2FrameOut> mfx_frame;
        res = AllocateFrame(work, mfx_frame);
        if (C2_OK != res) break;

        mfx_frame->PutC2Work(std::move(work));

        res = DecodeFrame(c2_bitstream_->GetFrameConstructor()->GetMfxBitstream().get(), std::move(mfx_frame));
        if (C2_OK != res) break;

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

    status_t res = C2_OK;

    do {

        std::unique_ptr<MfxC2FrameOut> mfx_frame;
        AllocateFrame(nullptr, mfx_frame); // mfx_frame without associated C2Work

        res = DecodeFrame(nullptr, std::move(mfx_frame));
    } while (C2_OK == res);

    if(C2_OK != res) {
        std::unique_ptr<MfxC2FrameOut> mfx_frame = surfaces_.AcquireUnlockedFrame();
        while(nullptr != mfx_frame) {
            std::unique_ptr<android::C2Work> work = mfx_frame->GetC2Work();
            if (work != nullptr) NotifyWorkDone(std::move(work), res);
            mfx_frame = surfaces_.AcquireUnlockedFrame();
        }
    }
}

void MfxC2DecoderComponent::WaitWork(std::unique_ptr<MfxC2FrameOut>&& frame, mfxSyncPoint sync_point)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    mfx_res = session_.SyncOperation(sync_point, MFX_C2_INFINITE);

    if (MFX_ERR_NONE != mfx_res) {
        MFX_DEBUG_TRACE_MSG("SyncOperation failed");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    }

    std::unique_ptr<android::C2Work> work = frame->GetC2Work();
    if (work) {
        if(MFX_ERR_NONE == mfx_res) {

            C2Event event;
            event.fire(); // pre-fire event as output buffer is ready to use

            mfxFrameSurface1 *mfx_surface = frame->GetMfxFrameSurface();
            MFX_DEBUG_TRACE_P(mfx_surface);

            if(!mfx_surface) mfx_res = MFX_ERR_NULL_PTR;
            else {
                MFX_DEBUG_TRACE_I32(mfx_surface->Data.Locked);
                MFX_DEBUG_TRACE_I64(mfx_surface->Data.TimeStamp);

                const C2Rect rect(mfx_surface->Info.CropW, mfx_surface->Info.CropH,
                                mfx_surface->Info.CropX, mfx_surface->Info.CropY);

                C2ConstGraphicBlock const_graphic = frame->GetC2GraphicBlock()->share(rect, event.fence());
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

status_t MfxC2DecoderComponent::queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items)
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
