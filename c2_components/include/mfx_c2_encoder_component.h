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
// m_ctrlOnce accumulates subsequent changes for one next frame.
// When AcquireEncodeCtrl is called it passes ownership to mfxEncodeCtrl
// and resets internal m_ctrlOnce,
// so next call will return nullptr (default) mfxEncodeCtrl.
class EncoderControl
{
private:
    // Encoder control for next one frame only.
    std::unique_ptr<mfxEncodeCtrl> m_ctrlOnce;

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
        ENCODER_VP9,
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

    mfxStatus InitEncoder();
    mfxStatus InitVPP(C2FrameData& buf_pack);
    // Allocate external system memory surface pool
    mfxStatus AllocateSurfacePool();
    void FreeSurfacePool();

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
    EncoderType m_encoderType;

    std::unique_ptr<MfxDev> m_device;

#ifdef USE_ONEVPL
    mfxSession m_mfxSession;
    mfxLoader m_mfxLoader;
#else
    MFXVideoSession m_mfxSession;
#endif

    // Accessed from working thread or stop method when working thread is stopped.
    std::unique_ptr<MFXVideoENCODE> m_mfxEncoder;

    // if custom allocator was set to session_ with SetFrameAllocator
    bool m_bAllocatorSet { false };

    MfxCmdQueue m_workingQueue;
    MFX_TRACEABLE(m_workingQueue);
    MfxCmdQueue m_waitingQueue;
    MFX_TRACEABLE(m_waitingQueue);

    // Video params configured through config_vb, retained between Start/Stop
    // sessions, used for init encoder,
    // can have zero (default) fields.
    MfxVideoParamsWrapper m_mfxVideoParamsConfig;
    // Internal encoder state, queried from encoder.
    MfxVideoParamsWrapper m_mfxVideoParamsState {};
    // Protects encoder initializatin and m_mfxVideoParamsConfig/m_mfxVideoParamsState
    mutable std::mutex m_initEncoderMutex;

    // Members handling MFX_WRN_DEVICE_BUSY.
    // Active sync points got from EncodeFrameAsync for waiting on.
    std::atomic_uint m_uSyncedPointsCount;
    // Condition variable to notify of decreasing active sync points.
    std::condition_variable m_devBusyCond;
    // Mutex to cover data accessed from condition variable checking lambda.
    // Even atomic type needs to be mutex protected.
    std::mutex m_devBusyMutex;

    // These are works whose input frames are sent to encoder,
    // got ERR_MORE_DATA so their output aren't being produced.
    // Handles display order only.
    // This queue is accessed from working thread only.
    std::queue<std::unique_ptr<C2Work>> m_pendingWorks;

    std::list<MfxC2FrameIn> m_lockedFrames;

    EncoderControl m_encoderControl;

    std::shared_ptr<C2BlockPool> m_c2Allocator;

    std::unique_ptr<BinaryWriter> m_outputWriter;

    bool m_bHeaderSent{false};

    mfxFrameSurface1 *m_encSrfPool;
    uint8_t *m_encOutBuf;
    uint32_t m_encSrfNum;

    // VPP used to convert color format which MSDK accepted.
    bool m_bVppDetermined;
    MfxC2VppWrapp m_vpp;
    MfxC2Conversion m_inputVppType;

    // Input frame info with width or height not 16byte aligned
    mfxFrameInfo m_mfxInputInfo;
};
