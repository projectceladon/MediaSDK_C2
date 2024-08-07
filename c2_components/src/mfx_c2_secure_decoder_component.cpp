// Copyright (c) 2017-2024 Intel Corporation
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

#include "mfx_c2_secure_decoder_component.h"

#include "mfx_defs.h"
#include "mfx_c2_utils.h"
#include "mfxstructures.h"
#include "mfxpcp.h"
#include <va/va_backend.h>

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_defaults.h"
#include "C2PlatformSupport.h"
#include <C2AllocatorGralloc.h>

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_secure_decoder_component"

constexpr c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;
constexpr mfxU16 DECRYPTION_BS_SIZE = 1024;
constexpr mfxU16 MAX_DECRYPTION_TASKS = 4;

MfxC2SecureDecoderComponent::MfxC2SecureDecoderComponent(const C2String name, const CreateConfig& config,
        std::shared_ptr<C2ReflectorHelper> reflector, DecoderType decoder_type) :
        MfxC2DecoderComponent(name, config, std::move(reflector), decoder_type)
{
    MFX_DEBUG_TRACE_FUNC;
    const unsigned int SINGLE_STREAM_ID = 0u;

    addParameter(
        DefineParam(m_secureMode, C2_PARAMKEY_SECURE_MODE)
        .withConstValue(new C2SecureModeTuning(C2Config::secure_mode_t::SM_READ_PROTECTED_WITH_ENCRYPTED))
        .build());
}

MfxC2SecureDecoderComponent::~MfxC2SecureDecoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    MfxC2DecoderComponent::Release();
}

void MfxC2SecureDecoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("c2.intel.avc.decoder.secure",
        &MfxC2Component::Factory<MfxC2SecureDecoderComponent, DecoderType>::Create<DECODER_H264_SECURE>);

    registry.RegisterMfxC2Component("c2.intel.hevc.decoder.secure",
        &MfxC2Component::Factory<MfxC2SecureDecoderComponent, DecoderType>::Create<DECODER_H265_SECURE>);
}

c2_status_t MfxC2SecureDecoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;
    c2_status_t c2_res = MfxC2DecoderComponent::Init();

    if(C2_OK == c2_res) {
        if (DECODER_H264_SECURE == m_decoderType)
            mfx_res = m_device->CheckHUCSupport(VAProfileH264ConstrainedBaseline)? MFX_ERR_NONE:MFX_ERR_UNSUPPORTED;
        else if (DECODER_H265_SECURE == m_decoderType)
            mfx_res = m_device->CheckHUCSupport(VAProfileHEVCMain) ? MFX_ERR_NONE:MFX_ERR_UNSUPPORTED;
        else
            mfx_res = MFX_ERR_UNSUPPORTED;
    } else {
        return c2_res;
    }

    return MfxStatusToC2(mfx_res);
}

c2_status_t MfxC2SecureDecoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t c2_res = MfxC2DecoderComponent::DoStart();

    if(C2_OK == c2_res) {
        m_decryptionQueue.Start();
    }

    return C2_OK;
}

c2_status_t MfxC2SecureDecoderComponent::DoStop(bool abort)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t c2_res = MfxC2DecoderComponent::DoStop(abort);

    if(C2_OK == c2_res) {
        if (abort) {
            m_decryptionQueue.Abort();
        } else {
            m_decryptionQueue.Stop();
        }
    }

    return C2_OK;
}

c2_status_t MfxC2SecureDecoderComponent::Queue(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& item : *items) {
        c2_status_t sts = ValidateWork(item);

        if (C2_OK == sts) {

            if (m_bEosReceived) { // All works following eos treated as errors.
                item->result = C2_BAD_VALUE;
                // Do this in working thread to follow Drain completion.
                m_workingQueue.Push( [work = std::move(item), this] () mutable {
                    PushPending(std::move(work));
                });
            } else {
                bool eos = (item->input.flags & C2FrameData::FLAG_END_OF_STREAM);
                bool empty = (item->input.buffers.size() == 0);
                if (eos) {
                    m_bEosReceived = true;
                }
                MFX_DEBUG_TRACE_I32(eos);
                MFX_DEBUG_TRACE_I32(empty);
                if (eos && empty) {
                    m_workingQueue.Push( [work = std::move(item), this] () mutable {
                        Drain(std::move(work));
                    });
                } else {
                    m_decryptionQueue.Push( [ work = std::move(item), this ] () mutable {
                        Decrypt(std::move(work));
                    } );
                    if (eos) {
                        m_workingQueue.Push( [this] () { Drain(nullptr); } );
                    }
                }
            }
        } else {
            NotifyWorkDone(std::move(item), sts);
        }
    }

    return C2_OK;
}

void MfxC2SecureDecoderComponent::Decrypt(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    mfxStatus mfx_res = MFX_ERR_NONE;

    const auto incoming_frame_index = work->input.ordinal.frameIndex;
    const auto incoming_flags = work->input.flags;

    MFX_DEBUG_TRACE_STREAM("work: " << work.get() << "; index: " << incoming_frame_index.peeku() <<
        " flags: " << std::hex << incoming_flags);

    do {
        std::unique_ptr<C2ReadView> read_view;
        res = m_c2Bitstream->AppendFrame(work->input, TIMEOUT_NS, &read_view);
        if (C2_OK != res) break;

        if (!CheckBitstream()) break;

        // Get VAContextID
        VAContextID contextId = VA_INVALID_ID;
        MFXVideoCORE_GetHandle(m_mfxSession, static_cast<mfxHandleType>(MFX_HANDLE_VA_CONTEXT_ID), (mfxHDL*)&contextId);
        if (contextId == VA_INVALID_ID) {
            mfx_res = MFX_ERR_INVALID_HANDLE;
            break;
        }

        DecryptionTask decryptionTask;
        mfx_res = SubmitDecryptionTask(contextId, ++m_decrytedFeedbackNumber, decryptionTask);
            if (MFX_ERR_MORE_DATA == mfx_res || MFX_WRN_DEVICE_BUSY == mfx_res)
                m_decrytedFeedbackNumber--;

        m_workingQueue.Push( [&work, &decryptionTask, this] () {
                            Dowork(std::move(work), std::move(decryptionTask), this->m_decrytedFeedbackNumber);
        } );
    } while(false);

}

void MfxC2SecureDecoderComponent::Dowork(std::unique_ptr<C2Work>&& work, DecryptionTask&& decryptionTask, mfxU32 feedbackNumber)
{
    MFX_DEBUG_TRACE_FUNC;
    if (MFX_ERR_NONE != WaitUtilDecryptionDone(decryptionTask, feedbackNumber)) return;

    mfxExtCencParam decryptParams;
    MFX_ZERO_MEMORY(decryptParams);
    decryptParams.Header.BufferId = MFX_EXTBUFF_CENC_PARAM;
    decryptParams.Header.BufferSz = sizeof(mfxExtCencParam);
    decryptParams.StatusReportIndex = feedbackNumber;

    mfxExtBuffer* pExtBuf = &decryptParams.Header;
    m_mfxVideoParams.ExtParam = &pExtBuf;
    m_mfxVideoParams.NumExtParam = 1;

    if (m_bFlushing) {
        m_flushedWorks.push_back(std::move(work));
        return;
    }

    c2_status_t res = C2_OK;
    mfxStatus mfx_sts = MFX_ERR_NONE;

    const auto incoming_frame_index = work->input.ordinal.frameIndex;
    const auto incoming_flags = work->input.flags;

    MFX_DEBUG_TRACE_STREAM("work: " << work.get() << "; index: " << incoming_frame_index.peeku() <<
        " flags: " << std::hex << incoming_flags);

    bool expect_output = false;
    bool flushing = false;
    bool codecConfig = ((incoming_flags & C2FrameData::FLAG_CODEC_CONFIG) != 0);
    // Av1 and VP9 don't need the bs which flag is config.
    if (codecConfig && (DECODER_AV1 == m_decoderType || DECODER_VP9 == m_decoderType)) {
        FillEmptyWork(std::move(work), C2_OK);
        if (true == m_bInitialized) {
            mfxStatus format_change_sts = HandleFormatChange();
            MFX_DEBUG_TRACE__mfxStatus(format_change_sts);
            mfx_sts = format_change_sts;
            if (MFX_ERR_NONE != mfx_sts) {
                FreeDecoder();
            }
        }
        return;
    }

    do {
        std::unique_ptr<C2ReadView> read_view;
        res = m_c2Bitstream->AppendFrame(work->input, TIMEOUT_NS, &read_view);
        if (C2_OK != res) break;

        {
            std::lock_guard<std::mutex> lock(m_readViewMutex);
            m_readViews.emplace(incoming_frame_index.peeku(), std::move(read_view));
            MFX_DEBUG_TRACE_I32(m_readViews.size());
        }

        if (work->input.buffers.size() == 0) break;

        PushPending(std::move(work));

        if (!m_c2Allocator) {
            res = GetCodec2BlockPool(m_outputPoolId,
                shared_from_this(), &m_c2Allocator);
            if (res != C2_OK) break;
#ifdef MFX_BUFFER_QUEUE
            bool hasSurface = std::static_pointer_cast<MfxC2BufferQueueBlockPool>(m_c2Allocator)->outputSurfaceSet();
            m_mfxVideoParams.IOPattern = hasSurface ? MFX_IOPATTERN_OUT_VIDEO_MEMORY : MFX_IOPATTERN_OUT_SYSTEM_MEMORY;
#endif
            if (m_mfxVideoParams.IOPattern == MFX_IOPATTERN_OUT_SYSTEM_MEMORY) {
                m_allocator = nullptr;
#ifdef USE_ONEVPL
                mfx_sts = MFXVideoCORE_SetFrameAllocator(m_mfxSession, nullptr);
#else
                mfx_sts = m_mfxSession.SetFrameAllocator(nullptr);
#endif
                m_bAllocatorSet = false;
                ALOGI("System memory is being used for decoding!");

                if (MFX_ERR_NONE != mfx_sts) break;
            }
        }

        // loop repeats DecodeFrame on the same frame
        // if DecodeFrame returns error which is repairable, like resolution change
        bool resolution_change = false;
        do {
            if (!m_bInitialized) {
                mfx_sts = InitDecoder(m_c2Allocator);
                if(MFX_ERR_NONE != mfx_sts) {
                    MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                    if (MFX_ERR_MORE_DATA == mfx_sts) {
                        mfx_sts = MFX_ERR_NONE; // not enough data for InitDecoder should not cause an error
                    }
                    res = MfxStatusToC2(mfx_sts);
                    break;
                }
                if (!m_bInitialized) {
                    MFX_DEBUG_TRACE_MSG("Cannot initialize mfx decoder");
                    res = C2_BAD_VALUE;
                    break;
                }
            }

            if (!m_bSetHdrStatic) UpdateHdrStaticInfo();

            mfxBitstream *bs = m_c2Bitstream->GetFrameConstructor()->GetMfxBitstream().get();
            MfxC2FrameOut frame_out;
            do {
                // check bitsream is empty
                if (bs && bs->DataLength == 0) {
                    mfx_sts = MFX_ERR_MORE_DATA;
                    break;
                }

                res = AllocateFrame(&frame_out);
                if (C2_OK != res) break;

                mfx_sts = DecodeFrame(bs, std::move(frame_out), &flushing, &expect_output);
            } while (mfx_sts == MFX_ERR_NONE || mfx_sts == MFX_ERR_MORE_SURFACE);

            if (MFX_ERR_MORE_DATA == mfx_sts) {
                mfx_sts = MFX_ERR_NONE; // valid result of DecodeFrame
            }

            resolution_change = (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == mfx_sts);
            if (resolution_change) {
                frame_out = MfxC2FrameOut(); // release the frame to be used in Drain

                Drain(nullptr);

                // Clear up all queue of works after drain except last work
                // which caused resolution change and should be used again.
                {
                    std::lock_guard<std::mutex> lock(m_pendingWorksMutex);
                    auto it = m_pendingWorks.begin();
                    while (it != m_pendingWorks.end()) {
                        if (it->first != incoming_frame_index) {
                            MFX_DEBUG_TRACE_STREAM("Work removed: " << NAMED(it->second->input.ordinal.frameIndex.peeku()));
                            NotifyWorkDone(std::move(it->second), C2_NOT_FOUND);
                            it = m_pendingWorks.erase(it);
                        } else {
                            ++it;
                        }
                    }
                }

                mfxStatus format_change_sts = HandleFormatChange();
                MFX_DEBUG_TRACE__mfxStatus(format_change_sts);
                mfx_sts = format_change_sts;
                if (MFX_ERR_NONE != mfx_sts) {
                    FreeDecoder();
                }
            }

        } while (resolution_change); // try again as it is a resolution change

        if (C2_OK != res) break; // if loop above was interrupted by C2 error

        if (MFX_ERR_NONE != mfx_sts) {
            MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
            res = MfxStatusToC2(mfx_sts);
            break;
        }

        res = m_c2Bitstream->Unload();
        if (C2_OK != res) break;

    } while(false); // fake loop to have a cleanup point there

    bool incomplete_frame =
        (incoming_flags & (C2FrameData::FLAG_INCOMPLETE | C2FrameData::FLAG_CODEC_CONFIG)) != 0;

    // sometimes the frame is split to several buffers with the same timestamp.
    incomplete_frame |= IsPartialFrame(incoming_frame_index.peeku());

    // notify listener in case of failure or empty output
    if (C2_OK != res || !expect_output || incomplete_frame || flushing) {
        if (!work) {
            std::lock_guard<std::mutex> lock(m_pendingWorksMutex);
            auto it = m_pendingWorks.find(incoming_frame_index);
            if (it != m_pendingWorks.end()) {
                work = std::move(it->second);
                m_pendingWorks.erase(it);
            } else {
                MFX_DEBUG_TRACE_STREAM("Not found C2Work, index = " << incoming_frame_index.peeku());
                // If not found, it might be removed by WaitWork. We don't need to return an error.
                // FatalError(C2_CORRUPTED);
            }
        }
        if (work) {
            if (flushing) {
                m_flushedWorks.push_back(std::move(work));
            } else {
                FillEmptyWork(std::move(work), res);
            }
        }
    }
}

bool MfxC2SecureDecoderComponent::CheckBitstream()
{
    MFX_DEBUG_TRACE_FUNC;

    auto bs = m_c2Bitstream->GetFrameConstructor()->GetMfxBitstream();

    if (!bs) return true;

    if ((bs->DataOffset + bs->DataLength) > bs->MaxLength)
        return false;

    if (bs->EncryptedData)
    {
        mfxEncryptedData * encryptedData = bs->EncryptedData;
        while (encryptedData)
        {
            if (!encryptedData->Data)
                return false;

            if (!encryptedData->DataLength || ((encryptedData->DataOffset + encryptedData->DataLength) > encryptedData->MaxLength))
                return false;

            encryptedData = encryptedData->Next;
        }
    }

    return true;
}

mfxStatus MfxC2SecureDecoderComponent::SubmitDecryptionTask(VAContextID contextId,
                                                            mfxU16 PESPacketCounter,
                                                            DecryptionTask& decryptionTask)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    // Get VADisplay
    VADisplay dpy = NULL;
    if (MFX_ERR_NONE == mfx_res)
    {
        mfx_res = MFXVideoCORE_GetHandle(m_mfxSession, static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL*)&dpy);
        if (MFX_ERR_NONE == mfx_res)
        {
            if (!dpy) mfx_res = MFX_ERR_INVALID_HANDLE;
        }
    }

    if (VA_INVALID_ID == contextId)
        mfx_res = MFX_ERR_NULL_PTR;

    auto m_pBitstream = m_c2Bitstream->GetFrameConstructor()->GetMfxBitstream();

    HUCVideoBuffer* pHucBuffer = NULL;
    if (MFX_ERR_NONE == mfx_res)
    {
        pHucBuffer = (HUCVideoBuffer*)(m_pBitstream->Data + m_pBitstream->DataOffset);
        if (!pHucBuffer)
            mfx_res = MFX_ERR_NULL_PTR;
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        if (pHucBuffer->drm_type == DRM_TYPE_CLASSIC_WV)
        {
            MFX_DEBUG_TRACE_MSG("DRM_TYPE_CLASSIC_WV");
            if (m_mfxVideoParams.Protected)
                mfx_res = (m_mfxVideoParams.Protected == MFX_PROTECTION_CENC_WV_CLASSIC) ? mfx_res : MFX_ERR_UNDEFINED_BEHAVIOR;
            else
                m_mfxVideoParams.Protected = MFX_PROTECTION_CENC_WV_CLASSIC;
        }
        else if (pHucBuffer->drm_type == DRM_TYPE_MDRM)
        {
            MFX_DEBUG_TRACE_MSG("DRM_TYPE_MDRM");
            if (m_mfxVideoParams.Protected)
                mfx_res = (m_mfxVideoParams.Protected == MFX_PROTECTION_CENC_WV_GOOGLE_DASH) ? mfx_res : MFX_ERR_UNDEFINED_BEHAVIOR;
            else
                m_mfxVideoParams.Protected = MFX_PROTECTION_CENC_WV_GOOGLE_DASH;
        }
        else
        {
            MFX_DEBUG_TRACE_MSG("Invalid DRM_TYPE");
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;
        }
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        if (!m_pBitstream->EncryptedData)
        {
            MFX_DEBUG_TRACE_MSG("No EncryptedData");
            mfx_res = MFX_ERR_NULL_PTR;
        }
    }

    mfxU8* data = NULL;
    mfxU32 dataSize = 0;
    if (MFX_ERR_NONE == mfx_res)
    {
        data = m_pBitstream->EncryptedData->Data;
        dataSize = m_pBitstream->EncryptedData->DataLength;
        if (!data || !dataSize)
        {
            MFX_DEBUG_TRACE_MSG("Not enough EncryptedData->Data or EncryptedData->DataLength");
            MFX_DEBUG_TRACE_P(m_pBitstream->EncryptedData->Data);
            MFX_DEBUG_TRACE_I32(m_pBitstream->EncryptedData->DataLength);
            mfx_res = MFX_ERR_MORE_DATA;
        }
    }

    // Fill decryption params
    VAEncryptionParameters PESInputParams;
    MFX_ZERO_MEMORY(PESInputParams);
    VAEncryptionSegmentInfo SegmentInfo[MAX_SUPPORTED_PACKETS];
    memset((mfxU8*)&(SegmentInfo[0]), 0, sizeof(VAEncryptionSegmentInfo)*MAX_SUPPORTED_PACKETS);
    if (MFX_ERR_NONE == mfx_res)
    {
        if (m_mfxVideoParams.Protected == MFX_PROTECTION_CENC_WV_CLASSIC)
        {
            PESInputParams.encryption_type = VA_ENCRYPTION_TYPE_FULLSAMPLE_CBC;
        }
        else if (m_mfxVideoParams.Protected == MFX_PROTECTION_CENC_WV_GOOGLE_DASH)
        {
            PESInputParams.encryption_type = VA_ENCRYPTION_TYPE_SUBSAMPLE_CTR;
        }
        else
            mfx_res = MFX_ERR_UNDEFINED_BEHAVIOR;

        PESInputParams.status_report_index = PESPacketCounter;
        // PESInputParams.session_id = m_pBitstream->EncryptedData->AppId;
        mfxU32 clearDataLength = 0;
        for(int i = 0; i < pHucBuffer->uiNumPackets; i++)
        {
            if (pHucBuffer->sPacketData[i].clear)
            {
                clearDataLength += pHucBuffer->sPacketData[i].clearPacketSize;
            }
            else
            {
                SegmentInfo[PESInputParams.num_segments].segment_start_offset   = pHucBuffer->sPacketData[i].sSegmentData.uiSegmentStartOffset - clearDataLength;
                SegmentInfo[PESInputParams.num_segments].segment_length         = pHucBuffer->sPacketData[i].sSegmentData.uiSegmentLength + clearDataLength;
                SegmentInfo[PESInputParams.num_segments].partial_aes_block_size = pHucBuffer->sPacketData[i].sSegmentData.uiPartialAesBlockSizeInBytes;
                SegmentInfo[PESInputParams.num_segments].init_byte_length       = clearDataLength;
                std::copy(std::begin(pHucBuffer->sPacketData[i].sSegmentData.uiAesIV),
                          std::end(pHucBuffer->sPacketData[i].sSegmentData.uiAesIV),
                          std::begin(SegmentInfo[PESInputParams.num_segments].aes_cbc_iv_or_ctr));

                PESInputParams.num_segments++;
                PESInputParams.segment_info = &(SegmentInfo[0]);

                clearDataLength = 0;
            }
        }
        PESInputParams.size_of_length = 0;//pHucBuffer->ucSizeOfLength; //WA: 0
    }

    VAStatus va_res = VA_STATUS_SUCCESS;
    VABufferID protectedSliceData = VA_INVALID_ID;
    VABufferID protectedParams = VA_INVALID_ID;
    mfxU8* buffer = NULL;

    // Submit bitstream for decryption
    if (MFX_ERR_NONE == mfx_res)
    {
        va_res = vaCreateBuffer(dpy,
                                contextId,
                                VAProtectedSliceDataBufferType,
                                dataSize,
                                1,
                                NULL,
                                &protectedSliceData);
        MFX_DEBUG_TRACE_MSG("vaCreateBuffer");
        MFX_DEBUG_TRACE_I32(va_res);
        mfx_res = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE != mfx_res)
        {
            MFX_DEBUG_TRACE_MSG("vaCreateBuffer() for protectedSliceData failed");
        }

        if (MFX_ERR_NONE == mfx_res)
        {
            va_res = vaMapBuffer(dpy, protectedSliceData, (void**)&buffer);
            MFX_DEBUG_TRACE_MSG("vaMapBuffer");
            MFX_DEBUG_TRACE_I32(va_res);
            mfx_res = va_to_mfx_status(va_res);
            if (MFX_ERR_NONE != mfx_res)
            {
                MFX_DEBUG_TRACE_MSG("vaMapBuffer() for protectedSliceData failed");
            }
            if (!buffer)
            {
                MFX_DEBUG_TRACE_MSG("vaMapBuffer() returned null buffer");
                mfx_res = MFX_ERR_MEMORY_ALLOC;
            }
        }

        if (MFX_ERR_NONE == mfx_res)
        {
            std::copy(data, data + dataSize, buffer);
            vaUnmapBuffer(dpy, protectedSliceData);
            MFX_DEBUG_TRACE_MSG("vaUnmapBuffer");
        }
    }

    // Submit headers
    if (MFX_ERR_NONE == mfx_res)
    {
        va_res = vaCreateBuffer(dpy,
                                contextId,
                                (VABufferType)VAEncryptionParameterBufferType,
                                sizeof(VAEncryptionParameters),
                                1,
                                NULL,
                                &protectedParams);
        MFX_DEBUG_TRACE_MSG("vaCreateBuffer");
        MFX_DEBUG_TRACE_I32(va_res);
        mfx_res = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE != mfx_res)
        {
            MFX_DEBUG_TRACE_MSG("vaCreateBuffer() for protectedParams failed");
        }

        if (MFX_ERR_NONE == mfx_res)
        {
            va_res = vaMapBuffer(dpy, protectedParams, (void**)&buffer);
            MFX_DEBUG_TRACE_MSG("vaMapBuffer");
            MFX_DEBUG_TRACE_I32(va_res);
            mfx_res = va_to_mfx_status(va_res);
            if (MFX_ERR_NONE != mfx_res)
            {
                MFX_DEBUG_TRACE_MSG("vaMapBuffer() for protectedParams failed");
            }
        }

        if (MFX_ERR_NONE == mfx_res)
        {
            uint8_t *src = reinterpret_cast<uint8_t*>(&PESInputParams);
            std::copy(src, src + sizeof(VAEncryptionParameters), buffer);
            vaUnmapBuffer(dpy, protectedParams);
            MFX_DEBUG_TRACE_MSG("vaUnmapBuffer");
        }
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        VABufferID buffers[2];
        buffers[0] = protectedSliceData;
        buffers[1] = protectedParams;

        va_res = vaRenderPicture(dpy, contextId, &buffers[0], 2);
        MFX_DEBUG_TRACE_MSG("vaRenderPicture");
        MFX_DEBUG_TRACE_I32(va_res);
        mfx_res = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE != mfx_res)
        {
            MFX_DEBUG_TRACE_MSG("vaRenderPicture() failed");
        }
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        VADisplayContextP pDisplayContext = (VADisplayContextP)dpy;
        VADriverContextP pDriverContext = pDisplayContext->pDriverContext;

        va_res = vaEndPicture(pDriverContext, contextId);
        MFX_DEBUG_TRACE_MSG("vaEndPicture");
        MFX_DEBUG_TRACE_I32(va_res);
        if (VA_STATUS_ERROR_SURFACE_BUSY == va_res)
        {
            mfx_res = MFX_WRN_DEVICE_BUSY;
            MFX_DEBUG_TRACE_MSG("vaEndPicture() returns device busy status");
        }
        else
        {
            mfx_res = va_to_mfx_status(va_res);
            if (MFX_ERR_NONE != mfx_res)
            {
                MFX_DEBUG_TRACE_MSG("vaEndPicture() failed");
            }
        }
    }

    if ((MFX_ERR_NONE == mfx_res || MFX_WRN_DEVICE_BUSY == mfx_res) && (VA_INVALID_ID != protectedParams))
    {
        va_res = vaDestroyBuffer(dpy, protectedParams);
        MFX_DEBUG_TRACE_MSG("vaDestroyBuffer");
        MFX_DEBUG_TRACE_I32(va_res);
        mfxStatus sts = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE != sts)
        {
            mfx_res = sts;
            MFX_DEBUG_TRACE_MSG("vaDestroyBuffer() for protectedParams failed");
        }
    }

    if (MFX_ERR_NONE == mfx_res)
    {
        decryptionTask.surfaceId = protectedSliceData;
        decryptionTask.usStatusReportFeedbackNumber = m_decrytedFeedbackNumber;
    } else {
        MFX_DEBUG_TRACE_STREAM("SubmitDecryptionTask failed, mfx_res = " << mfx_res);
        va_res = vaDestroyBuffer(dpy, protectedSliceData);
        mfxStatus sts = va_to_mfx_status(va_res);
        if (MFX_ERR_NONE != sts)
        {
            mfx_res = sts;
            MFX_DEBUG_TRACE_MSG("vaDestroyBuffer() for protectedSliceData failed");
        }
    }

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2SecureDecoderComponent::WaitUtilDecryptionDone(DecryptionTask& decryptionTask, mfxU32 feedbackNumber)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    // Get VADisplay
    VADisplay dpy = NULL;
    MFXVideoCORE_GetHandle(m_mfxSession, static_cast<mfxHandleType>(MFX_HANDLE_VA_DISPLAY), (mfxHDL*)&dpy);

    do {
        if (!dpy) {
            mfx_res = MFX_ERR_INVALID_HANDLE;
            break;
        }
        auto va_res = vaSyncSurface(dpy, decryptionTask.surfaceId);
        mfx_res = va_to_mfx_status(va_res);
    } while (false);

    MFX_DEBUG_TRACE_I32(mfx_res);
    return mfx_res;
}
