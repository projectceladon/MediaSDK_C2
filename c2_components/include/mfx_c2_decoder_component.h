// Copyright (c) 2017-2022 Intel Corporation
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
#include "mfx_c2_frame_out.h"
#include "mfx_c2_bitstream_in.h"
#include "mfx_frame_pool_allocator.h"
#include "mfx_gralloc_allocator.h"
#include "mfx_c2_color_aspects_wrapper.h"
#include "mfx_c2_setters.h"
#include <cutils/properties.h>

class MfxC2DecoderComponent : public MfxC2Component
{
public:
    enum DecoderType {
        DECODER_H264,
        DECODER_H265,
        DECODER_VP9,
        DECODER_VP8,
        DECODER_MPEG2,
        DECODER_AV1,
    };

    enum class OperationState {
        INITIALIZATION,
        RUNNING,
        STOPPING,
        STOPPED,
    };

protected:
    MfxC2DecoderComponent(const C2String name, const CreateConfig& config,
        std::shared_ptr<C2ReflectorHelper> reflector, DecoderType decoder_type);

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

    c2_status_t UpdateMfxParamToC2(
        std::unique_lock<std::mutex> state_lock,
        const std::vector<C2Param*> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        c2_blocking_t mayBlock,
        std::vector<std::unique_ptr<C2Param>>* const heapParams) const override;

    c2_status_t UpdateC2ParamToMfx(
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
    c2_status_t UpdateC2Param(const mfxVideoParam* src, C2Param::Index index) const;

    void DoUpdateMfxParam(const std::vector<C2Param*> &params,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures,
        bool queue_update);

    void InitFrameConstructor();

    mfxStatus InitSession();

    mfxStatus ResetSettings();

    mfxStatus InitDecoder(std::shared_ptr<C2BlockPool> c2_allocator);

    void FreeDecoder();

    void FreeSurfaces();

    mfxStatus HandleFormatChange();

    mfxStatus DecodeFrameAsync(
        mfxBitstream *bs, mfxFrameSurface1 *surface_work, mfxFrameSurface1 **surface_out,
        mfxSyncPoint *syncp);

    mfxStatus DecodeFrame(mfxBitstream *bs, MfxC2FrameOut&& frame_out,
        bool* flushing, bool* expect_output);

    c2_status_t AllocateC2Block(uint32_t width, uint32_t height, uint32_t fourcc, std::shared_ptr<C2GraphicBlock>* out_block);

    c2_status_t AllocateFrame(MfxC2FrameOut* frame_out);

    mfxU16 GetAsyncDepth();

    // Work routines
    c2_status_t ValidateWork(const std::unique_ptr<C2Work>& work);

    void DoWork(std::unique_ptr<C2Work>&& work);

    void ReleaseReadViews(uint64_t incoming_frame_index);

    void EmptyReadViews(uint64_t timestamp, uint64_t frame_index);

    bool IsPartialFrame(uint64_t frame_index);

    bool IsDuplicatedTimeStamp(uint64_t timestamp);

    void FillEmptyWork(std::unique_ptr<C2Work>&& work, c2_status_t res);

    void Drain(std::unique_ptr<C2Work>&& work);
    // waits for the sync_point and update work with decoder output then
    void WaitWork(MfxC2FrameOut&& frame_out, mfxSyncPoint sync_point);

    void PushPending(std::unique_ptr<C2Work>&& work);

    void UpdateHdrStaticInfo();

    std::shared_ptr<C2StreamColorAspectsInfo::output> getColorAspects_l() const;

private:
    DecoderType m_decoderType;

    std::unique_ptr<MfxDev> m_device;
#ifdef USE_ONEVPL
    mfxSession m_mfxSession;
    mfxLoader m_mfxLoader;
#else
    MFXVideoSession m_mfxSession;
#endif

    // Accessed from working thread or stop method when working thread is stopped.
    std::unique_ptr<MFXVideoDECODE> m_mfxDecoder;

    // if custom allocator was set to m_session with SetFrameAllocator
    bool m_bAllocatorSet { false };

    bool m_bInitialized;

    OperationState m_OperationState { OperationState::INITIALIZATION };

    MfxCmdQueue m_workingQueue;
    MFX_TRACEABLE(m_workingQueue);
    MfxCmdQueue m_waitingQueue;
    MFX_TRACEABLE(m_waitingQueue);

    mfxVideoParam m_mfxVideoParams {};
    std::vector<mfxExtBuffer*> m_extBuffers;
    mfxExtVideoSignalInfo m_signalInfo;

    // Protects decoder initialization and m_mfxVideoParams
    mutable std::mutex m_initDecoderMutex;
    // Width and height of decoding surfaces and respectively maximum frame size supported
    // without re-creation of decoder when resolution changed.
    mfxU16 m_uMaxWidth {};
    mfxU16 m_uMaxHeight {};

    std::atomic<bool> m_bEosReceived {};
    // Members handling MFX_WRN_DEVICE_BUSY.
    // Active sync points got from DecodeFrameAsync for waiting on.
    std::atomic_uint m_uSyncedPointsCount;
    // Condition variable to notify of decreasing active sync points.
    std::condition_variable m_devBusyCond;
    // Mutex to cover data accessed from condition variable checking lambda.
    // Even atomic type needs to be mutex protected.
    std::mutex m_devBusyMutex;

    std::unique_ptr<MfxC2BitstreamIn> m_c2Bitstream;
    // Store raw pointers there as don't want to keep objects by shared_ptr
    std::map<uint64_t, std::shared_ptr<mfxFrameSurface1>> m_surfaces; // all ever send to Decoder
    // Store all surfaces used for system memory
    std::list<std::pair<mfxFrameSurface1*, std::shared_ptr<C2GraphicBlock>>> m_blocks;

    std::mutex m_lockedSurfacesMutex;
    std::list<MfxC2FrameOut> m_lockedSurfaces; // allocated, but cannot be re-used as Locked by Decoder

    std::mutex m_pendingWorksMutex;
    std::map<decltype(C2WorkOrdinalStruct::timestamp), std::unique_ptr<C2Work>> m_pendingWorks;

    std::shared_ptr<MfxFramePoolAllocator> m_allocator; // used when Video memory output
    // for pre-allocation when Video memory is chosen and always when System memory output
    std::shared_ptr<C2BlockPool> m_c2Allocator;
    C2BlockPool::local_id_t m_outputPoolId = C2BlockPool::PLATFORM_START;
    std::unique_ptr<MfxGrallocAllocator> m_grallocAllocator;
    std::atomic<bool> m_bFlushing{false};

    std::list<std::unique_ptr<C2Work>> m_flushedWorks;

    std::mutex m_readViewMutex;
    std::map<const uint64_t, std::unique_ptr<C2ReadView>> m_readViews;
    std::list<std::pair<uint64_t, uint64_t>> m_duplicatedTimeStamp;

    std::shared_ptr<C2StreamHdrStaticInfo::output> m_hdrStaticInfo;
    bool m_bSetHdrStatic;

    MfxC2ColorAspectsWrapper m_colorAspectsWrapper;

    std::shared_ptr<C2StreamPixelFormatInfo::output> m_pixelFormat;

    std::vector<std::unique_ptr<C2Param>> m_updatingC2Configures;

    uint64_t m_consumerUsage;

    uint32_t m_surfaceNum;
    std::list<std::shared_ptr<mfxFrameSurface1>> m_surfacePool; // used in case of system memory

    unsigned int m_uOutputDelay = 8u;
    unsigned int m_uInputDelay = 0u;

#if MFX_DEBUG_DUMP_FRAME == MFX_DEBUG_YES
    int m_count = 0;
    std::mutex m_count_lock;
    bool NeedDumpBuffer();
#endif

    /* -----------------------C2Parameters--------------------------- */
    std::shared_ptr<C2ComponentNameSetting> m_name;
    std::shared_ptr<C2ComponentKindSetting> m_kind;
    std::shared_ptr<C2ComponentDomainSetting> m_domain;
    std::shared_ptr<C2StreamPictureSizeInfo::output> m_size;
    std::shared_ptr<C2PortSurfaceAllocatorTuning::output> m_surfaceAllocator;
    std::shared_ptr<C2PortAllocatorsTuning::input> m_inputAllocators;
    std::shared_ptr<C2PortAllocatorsTuning::output> m_outputAllocators;
    std::shared_ptr<C2StreamMaxPictureSizeTuning::output> m_maxSize;
    std::shared_ptr<C2StreamMaxBufferSizeInfo::input> m_maxInputSize;
    std::shared_ptr<C2PortMediaTypeSetting::output> m_outputMediaType;
    std::shared_ptr<C2PortRequestedDelayTuning::output> m_requestedOutputDelay;
    std::shared_ptr<C2PortBlockPoolsTuning::output> m_outputPoolIds;
    std::shared_ptr<C2PortMediaTypeSetting::input> m_inputMediaType;
    std::shared_ptr<C2StreamBufferTypeSetting::input> m_inputFormat;
    std::shared_ptr<C2StreamBufferTypeSetting::output> m_outputFormat;
    std::shared_ptr<C2StreamProfileLevelInfo::input> m_profileLevel;
    std::shared_ptr<C2PortActualDelayTuning::output> m_actualOutputDelay;
    std::shared_ptr<C2PortRequestedDelayTuning::input> m_requestedInputDelay;
    std::shared_ptr<C2PortActualDelayTuning::input> m_actualInputDelay;
    std::shared_ptr<C2PortDelayTuning::input> m_inputDelay;
    std::shared_ptr<C2StreamColorAspectsTuning::output> m_defaultColorAspects;
    std::shared_ptr<C2StreamColorAspectsInfo::input> m_codedColorAspects;
    std::shared_ptr<C2StreamColorAspectsInfo::output> m_colorAspects;
    /* ----------------------------------------Setters------------------------------------------- */
    static C2R OutputSurfaceAllocatorSetter(bool mayBlock, C2P<C2PortSurfaceAllocatorTuning::output> &me);
    static C2R SizeSetter(bool mayBlock, const C2P<C2StreamPictureSizeInfo::output> &oldMe,
                        C2P<C2StreamPictureSizeInfo::output> &me);
    static C2R MaxPictureSizeSetter(bool mayBlock, C2P<C2StreamMaxPictureSizeTuning::output> &me,
                                const C2P<C2StreamPictureSizeInfo::output> &size);
    static C2R MaxInputSizeSetter(bool mayBlock, C2P<C2StreamMaxBufferSizeInfo::input> &me,
                                const C2P<C2StreamMaxPictureSizeTuning::output> &maxSize);
    static C2R ProfileLevelSetter(bool mayBlock, C2P<C2StreamProfileLevelInfo::input> &me,
                                  const C2P<C2StreamPictureSizeInfo::output> &size);
    static C2R DefaultColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsTuning::output> &me);
    static C2R CodedColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::input> &me);
    static C2R ColorAspectsSetter(bool mayBlock, C2P<C2StreamColorAspectsInfo::output> &me,
                                const C2P<C2StreamColorAspectsTuning::output> &def,
                                const C2P<C2StreamColorAspectsInfo::input> &coded);
};
