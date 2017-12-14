/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

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
    };

protected:
    MfxC2DecoderComponent(const android::C2String name, int flags, DecoderType decoder_type);

    MFX_CLASS_NO_COPY(MfxC2DecoderComponent)

public:
    virtual ~MfxC2DecoderComponent();

public:
    static void RegisterClass(MfxC2ComponentsRegistry& registry);

protected: // android::C2Component
    status_t queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items) override;

protected:
    android::status_t Init() override;

    android::status_t DoStart() override;

    android::status_t DoStop() override;

private:
    mfxStatus InitSession();

    mfxStatus Reset();

    mfxStatus InitDecoder(std::shared_ptr<android::C2BlockAllocator> c2_allocator);

    void FreeDecoder();

    mfxStatus DecodeFrameAsync(
        mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,
        mfxSyncPoint *syncp);

    status_t DecodeFrame(mfxBitstream *bs, std::unique_ptr<MfxC2FrameOut>&& mfx_frame);

    android::status_t AllocateFrame(const std::unique_ptr<android::C2Work>& work,
        std::unique_ptr<MfxC2FrameOut>& mfx_frame);

    mfxU16 GetAsyncDepth();

    // Work routines
    void DoWork(std::unique_ptr<android::C2Work>&& work);

    void Drain();
    // waits for the sync_point and update work with decoder output then
    void WaitWork(std::unique_ptr<MfxC2FrameOut>&& frame, mfxSyncPoint sync_point);

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

    // Members handling MFX_WRN_DEVICE_BUSY.
    // Active sync points got from DecodeFrameAsync for waiting on.
    std::atomic_uint synced_points_count_;
    // Condition variable to notify of decreasing active sync points.
    std::condition_variable dev_busy_cond_;
    // Mutex to cover data accessed from condition variable checking lambda.
    // Even atomic type needs to be mutex protected.
    std::mutex dev_busy_mutex_;

    std::unique_ptr<MfxC2BitstreamIn> c2_bitstream_;

    MfxC2FrameOutPool surfaces_;

    MfxFramePoolAllocator* allocator_;
};
