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

#pragma once

#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_dev.h"
#include "mfx_cmd_queue.h"
#include "mfx_c2_frame_in.h"
#include "mfx_c2_bitstream_out.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_vpp_wrapp.h"

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
        ENCODER_H265,
    };

protected:
    MfxC2EncoderComponent(const C2String name, const CreateConfig& config,
        std::shared_ptr<MfxC2ParamReflector> reflector, EncoderType encoder_type);

    MFX_CLASS_NO_COPY(MfxC2EncoderComponent)

public:
    virtual ~MfxC2EncoderComponent();

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

    c2_status_t Queue(std::list<std::unique_ptr<C2Work>>* const items) override;

private:
    c2_status_t QueryParam(const mfxVideoParam* src,
        C2Param::Index index, C2Param** dst) const;

    std::unique_ptr<mfxVideoParam> GetParamsView() const;

    mfxStatus InitSession();

    mfxStatus ResetSettings();

    mfxStatus InitEncoder(const mfxFrameInfo& frame_info);
    mfxStatus InitVPP(C2FrameData& buf_pack);

    void FreeEncoder();

    void RetainLockedFrame(MfxC2FrameIn&& input);

    mfxStatus EncodeFrameAsync(
        mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs,
        mfxSyncPoint *syncp);

    c2_status_t AllocateBitstream(const std::unique_ptr<C2Work>& work,
        MfxC2BitstreamOut* mfx_bitstream);

    void DoConfig(const std::vector<C2Param*> &params,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures,
        bool queue_update);

    c2_status_t ApplyWorkTunings(C2Work& work);
    // Work routines
    void DoWork(std::unique_ptr<C2Work>&& work);

    void Drain(std::unique_ptr<C2Work>&& work);

    void ReturnEmptyWork(std::unique_ptr<C2Work>&& work);
    // waits for the sync_point and update work with encoder output then
    void WaitWork(std::unique_ptr<C2Work>&& work,
        std::unique_ptr<mfxEncodeCtrl>&& encode_ctrl,
        MfxC2BitstreamOut&& bit_stream, mfxSyncPoint sync_point);

private:
    EncoderType encoder_type_;

    std::unique_ptr<MfxDev> device_;

#ifdef USE_ONEVPL
    mfxSession m_mfxSession;
    mfxLoader m_mfxLoader;
#else
    MFXVideoSession session_;
#endif

    // Accessed from working thread or stop method when working thread is stopped.
    std::unique_ptr<MFXVideoENCODE> encoder_;

    // if custom allocator was set to session_ with SetFrameAllocator
    bool allocator_set_ { false };

    MfxCmdQueue working_queue_;
    MFX_TRACEABLE(working_queue_);
    MfxCmdQueue waiting_queue_;
    MFX_TRACEABLE(waiting_queue_);

    // Video params configured through config_vb, retained between Start/Stop
    // sessions, used for init encoder,
    // can have zero (default) fields.
    MfxVideoParamsWrapper video_params_config_;
    // Internal encoder state, queried from encoder.
    MfxVideoParamsWrapper video_params_state_ {};
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
    std::queue<std::unique_ptr<C2Work>> pending_works_;

    std::list<MfxC2FrameIn> locked_frames_;

    EncoderControl encoder_control_;

    std::shared_ptr<C2BlockPool> c2_allocator_;

    std::unique_ptr<BinaryWriter> output_writer_;

    bool header_sent_{false};

    // VPP used to convert color format which MSDK accepted.
    bool vpp_determined_;
    MfxC2VppWrapp vpp_;
    MfxC2Conversion input_vpp_type_;
};
