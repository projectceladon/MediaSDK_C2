/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_dev.h"
#include "mfx_cmd_queue.h"
#include "mfx_c2_frame_out.h"
#include "mfx_c2_bitstream_in.h"
#include "mfx_frame_pool_allocator.h"

class MfxC2DecoderComponent : public MfxC2Component
{
public:
    enum DecoderType {
        DECODER_H264,
        DECODER_H265,
        DECODER_VP9,
    };

protected:
    MfxC2DecoderComponent(const C2String name, const CreateConfig& config,
        std::shared_ptr<MfxC2ParamReflector> reflector, DecoderType decoder_type);

    MFX_CLASS_NO_COPY(MfxC2DecoderComponent)

public:
    virtual ~MfxC2DecoderComponent();

public:
    static void RegisterClass(MfxC2ComponentsRegistry& registry);

protected:
    c2_status_t Init() override;

    c2_status_t DoStart() override;

    c2_status_t DoStop(bool abort) override;

    c2_status_t Release() override;

    c2_status_t Query(
        std::unique_lock<std::mutex> state_lock,
        const std::vector<C2Param*> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        c2_blocking_t mayBlock,
        std::vector<std::unique_ptr<C2Param>>* const heapParams) const override;

    c2_status_t Config(
        std::unique_lock<std::mutex> state_lock,
        const std::vector<C2Param*> &params,
        c2_blocking_t mayBlock,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures) override;

    /*
    C2Work input->output relationship is estalished like it is done in C2 software avc decoder:
    1) If incoming work bitstream is decoded to frame with the same frame index ->
        return it within the same C2Work (frame index is passed as pts to find matching output).
    2) If output frame index doesn't match input -> look for matching one in pending works collection.
    3) If decoder produced frame for future refs, wait for more date -> put incoming work to pending works collection
        (to be used on step 2 in a next pass).
    4) If decoder did not produce anything, just demands more bitstream ->
        return empty C2Work: no output buffers, worklets processed = 1
    3) and 4) could be distinguished by incoming C2FrameData::flags: if it has FLAG_INCOMPLETE or FLAG_CODEC_CONFIG,
        then no output is expected.
        DecodeFrameAsync return code or surface_work->Locked on its exit don't provide correct
        information about output expectation.
    */

    c2_status_t Queue(std::list<std::unique_ptr<C2Work>>* const items) override;

    c2_status_t Flush(std::list<std::unique_ptr<C2Work>>* const flushedWork) override;

private:
    c2_status_t QueryParam(const mfxVideoParam* src,
        C2Param::Index index, C2Param** dst) const;

    void DoConfig(const std::vector<C2Param*> &params,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures,
        bool queue_update);

    void InitFrameConstructor();

    mfxStatus InitSession();

    mfxStatus Reset();

    mfxStatus InitDecoder(std::shared_ptr<C2BlockPool> c2_allocator);

    void FreeDecoder();

    mfxStatus DecodeFrameAsync(
        mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,
        mfxSyncPoint *syncp);

    mfxStatus DecodeFrame(mfxBitstream *bs, MfxC2FrameOut&& frame_out,
        bool* flushing, bool* expect_output);

    c2_status_t AllocateC2Block(uint32_t width, uint32_t height, std::shared_ptr<C2GraphicBlock>* out_block);

    c2_status_t AllocateFrame(MfxC2FrameOut* frame_out);

    mfxU16 GetAsyncDepth();

    // Work routines
    c2_status_t ValidateWork(const std::unique_ptr<C2Work>& work);

    void DoWork(std::unique_ptr<C2Work>&& work);

    void Drain(std::unique_ptr<C2Work>&& work);
    // waits for the sync_point and update work with decoder output then
    void WaitWork(MfxC2FrameOut&& frame_out, mfxSyncPoint sync_point);

    void PushPending(std::unique_ptr<C2Work>&& work);

private:
    DecoderType decoder_type_;

    std::unique_ptr<MfxDev> device_;
    MFXVideoSession session_;
    // if custom allocator was set to session_ with SetFrameAllocator
    bool allocator_set_ { false };

    // Accessed from working thread or stop method when working thread is stopped.
    std::unique_ptr<MFXVideoDECODE> decoder_;
    bool initialized_;

    MfxCmdQueue working_queue_;
    MFX_TRACEABLE(working_queue_);
    MfxCmdQueue waiting_queue_;
    MFX_TRACEABLE(waiting_queue_);

    mfxVideoParam video_params_ {};
    // Protects decoder initialization and video_params_
    mutable std::mutex init_decoder_mutex_;
    // Width and height of decoding surfaces and respectively maximum frame size supported
    // without re-creation of decoder when resolution changed.
    mfxU16 max_width_ {};
    mfxU16 max_height_ {};

    std::atomic<bool> eos_received_ {};
    // Members handling MFX_WRN_DEVICE_BUSY.
    // Active sync points got from DecodeFrameAsync for waiting on.
    std::atomic_uint synced_points_count_;
    // Condition variable to notify of decreasing active sync points.
    std::condition_variable dev_busy_cond_;
    // Mutex to cover data accessed from condition variable checking lambda.
    // Even atomic type needs to be mutex protected.
    std::mutex dev_busy_mutex_;

    std::unique_ptr<MfxC2BitstreamIn> c2_bitstream_;

    std::map<const C2Handle*, std::shared_ptr<mfxFrameSurface1>> surfaces_; // all ever send to Decoder

    std::mutex locked_surfaces_mutex_;
    std::list<MfxC2FrameOut> locked_surfaces_; // allocated, but cannot be re-used as Locked by Decoder

    std::mutex pending_works_mutex_;
    std::map<decltype(C2WorkOrdinalStruct::frameIndex), std::unique_ptr<C2Work>> pending_works_;

    std::shared_ptr<MfxFramePoolAllocator> allocator_; // used when Video memory output
    // for pre-allocation when Video memory is chosen and always when System memory output
    std::shared_ptr<C2BlockPool> c2_allocator_;

    std::atomic<bool> flushing_{false};

    std::list<std::unique_ptr<C2Work>> flushed_works_;
};
