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
#include "mfx_c2_frame_in.h"
#include "mfx_c2_bitstream_out.h"

// Assumes all calls are done from one (working) thread, no sync is needed.
// ctrl_once_ accumulates subsequent changes for one next frame.
// When AcquireEncodeCtrl is called it passes ownership to mfxEncodeCtrl
// and resets internal ctrl_once_,
// so next call will return nullptr (default) mfxEncodeCtrl.
class EncoderControl
{
private:
    // Encoder control for next one frame only.
    std::unique_ptr<mfxEncodeCtrl> ctrl_once_;

public:
    typedef std::function<void(mfxEncodeCtrl* ctrl)> ModifyFunction;

    void Modify(ModifyFunction& function);

    std::unique_ptr<mfxEncodeCtrl> AcquireEncodeCtrl();
};

class MfxC2EncoderComponent : public MfxC2Component
{
public:
    enum EncoderType {
        ENCODER_H264,
    };

protected:
    MfxC2EncoderComponent(const android::C2String name, int flags, EncoderType encoder_type);

    MFX_CLASS_NO_COPY(MfxC2EncoderComponent)

public:
    virtual ~MfxC2EncoderComponent();

public:
    static void RegisterClass(MfxC2ComponentsRegistry& registry);

protected: // android::C2ComponentInterface
    status_t query_nb(
        const std::vector<android::C2Param* const> &stackParams,
        const std::vector<android::C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<android::C2Param>>* const heapParams) const override;

    status_t config_nb(
            const std::vector<android::C2Param* const> &params,
            std::vector<std::unique_ptr<android::C2SettingResult>>* const failures) override;

protected: // android::C2Component
    status_t queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items) override;

protected:
    android::status_t Init() override;

    android::status_t DoStart() override;

    android::status_t DoStop() override;

private:
    status_t QueryParam(const mfxVideoParam* src,
        android::C2Param::Type type, android::C2Param** dst) const;

    std::unique_ptr<mfxVideoParam> GetParamsView() const;

    mfxStatus Reset();

    mfxStatus InitEncoder(const mfxFrameInfo& frame_info);

    void FreeEncoder();

    void RetainLockedFrame(MfxC2FrameIn input);

    mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs,
        mfxSyncPoint *syncp);

    android::status_t AllocateBitstream(const std::unique_ptr<android::C2Work>& work,
        MfxC2BitstreamOut* mfx_bitstream);

    // Work routines
    void DoWork(std::unique_ptr<android::C2Work>&& work);

    void Drain();
    // waits for the sync_point and update work with encoder output then
    void WaitWork(std::unique_ptr<android::C2Work>&& work,
        std::unique_ptr<mfxEncodeCtrl>&& encode_ctrl,
        MfxC2BitstreamOut&& bit_stream, mfxSyncPoint sync_point);

private:
    EncoderType encoder_type_;

    std::unique_ptr<MfxDev> device_;
    MFXVideoSession session_;

    // Accessed from working thread or stop method when working thread is stopped.
    std::unique_ptr<MFXVideoENCODE> encoder_;

    MfxCmdQueue working_queue_;
    MfxCmdQueue waiting_queue_;

    // Video params configured through config_nb, retained between Start/Stop
    // sessions, used for init encoder,
    // can have zero (default) fields.
    mfxVideoParam video_params_config_;
    // Internal encoder state, queried from encoder.
    mfxVideoParam video_params_state_ {};
    // Protects encoder initializatin and video_params_config_/video_params_state_
    mutable std::mutex init_encoder_mutex_;

    // Members handling MFX_WRN_DEVICE_BUSY.
    // Active sync points got from EncodeFrameAsync for waiting on.
    std::atomic_uint synced_points_count_;
    // Condition variable to notify of decreasing active sync points.
    std::condition_variable dev_busy_cond_;
    // Mutex to cover data accessed from condition variable checking lambda.
    // Even atomic type needs to be mutex protected.
    std::mutex dev_busy_mutex_;

    // These are works whose input frames are sent to encoder,
    // got ERR_MORE_DATA so their output aren't being produced.
    // Handles display order only.
    // This queue is accessed from working thread only.
    std::queue<std::unique_ptr<android::C2Work>> pending_works_;

    std::list<MfxC2FrameIn> locked_frames_;

    EncoderControl encoder_control_;
};
