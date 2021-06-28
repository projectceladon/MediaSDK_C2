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
#include "mfx_c2_frame_out.h"
#include "mfx_c2_bitstream_in.h"
#include "mfx_frame_pool_allocator.h"
#include "mfx_gralloc_allocator.h"
#include "mfx_c2_color_aspects_wrapper.h"

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

    mfxStatus ResetSettings();

    mfxStatus InitDecoder(std::shared_ptr<C2BlockPool> c2_allocator);

    void FreeDecoder();

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

    void Drain(std::unique_ptr<C2Work>&& work);
    // waits for the sync_point and update work with decoder output then
    void WaitWork(MfxC2FrameOut&& frame_out, mfxSyncPoint sync_point);

    void PushPending(std::unique_ptr<C2Work>&& work);

    void UpdateHdrStaticInfo();

    std::shared_ptr<C2StreamColorAspectsInfo::output> getColorAspects_l();

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
    std::vector<mfxExtBuffer*> ext_buffers_;
    mfxExtVideoSignalInfo signal_info_;

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
    // Store raw pointers there as don't want to keep objects by shared_ptr
    std::map<uint64_t, std::shared_ptr<mfxFrameSurface1>> surfaces_; // all ever send to Decoder

    std::mutex locked_surfaces_mutex_;
    std::list<MfxC2FrameOut> locked_surfaces_; // allocated, but cannot be re-used as Locked by Decoder
    std::list<std::shared_ptr<C2GraphicBlock>> locked_block_; //locked block, don't use

    std::mutex pending_works_mutex_;
    std::map<decltype(C2WorkOrdinalStruct::timestamp), std::unique_ptr<C2Work>> pending_works_;

    std::shared_ptr<MfxFramePoolAllocator> allocator_; // used when Video memory output
    // for pre-allocation when Video memory is chosen and always when System memory output
    std::shared_ptr<C2BlockPool> c2_allocator_;
    C2BlockPool::local_id_t output_pool_id_ = C2BlockPool::BASIC_GRAPHIC;
    std::unique_ptr<MfxGrallocAllocator> gralloc_allocator_;
    std::atomic<bool> flushing_{false};

    std::list<std::unique_ptr<C2Work>> flushed_works_;

    C2StreamHdrStaticInfo::output hdr_static_info_;
    bool set_hdr_sei_;

    MfxC2ColorAspectsWrapper color_aspects_;

    unsigned int output_delay_ = 8u;
    unsigned int input_delay_ = 1u;
};
