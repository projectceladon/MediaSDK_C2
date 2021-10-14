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

#include "mfx_c2_encoder_component.h"

#include "mfx_debug.h"
#include "mfx_msdk_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_params.h"
#include "mfx_defaults.h"
#include "C2PlatformSupport.h"
#include "mfx_gralloc_allocator.h"

#include <limits>
#include <thread>
#include <chrono>
#include <iomanip>
#include <C2AllocatorGralloc.h>
#include <C2Config.h>
#include <Codec2Mapper.h>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_encoder_component"

const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;
const mfxU32 MFX_MAX_H264_FRAMERATE = 172;
const mfxU32 MFX_MAX_H265_FRAMERATE = 300;

std::unique_ptr<mfxEncodeCtrl> EncoderControl::AcquireEncodeCtrl()
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<mfxEncodeCtrl> res;

    if (nullptr != m_ctrlOnce) {
        res = std::move(m_ctrlOnce);
    } // otherwise return nullptr
    return res;
}

void EncoderControl::Modify(ModifyFunction& function)
{
    MFX_DEBUG_TRACE_FUNC;

    // modify ctrl_once, create if null
    if (nullptr == m_ctrlOnce) {
        m_ctrlOnce = std::make_unique<mfxEncodeCtrl>();
    }
    function(m_ctrlOnce.get());
}

MfxC2EncoderComponent::MfxC2EncoderComponent(const C2String name, const CreateConfig& config,
    std::shared_ptr<MfxC2ParamReflector> reflector, EncoderType encoder_type) :
        MfxC2Component(name, config, std::move(reflector)),
        m_encoderType(encoder_type),
#ifdef USE_ONEVPL
        m_mfxSession(nullptr),
        m_mfxLoader(nullptr),
#endif
        m_uSyncedPointsCount(0),
        m_bVppDetermined(false),
        m_inputVppType(CONVERT_NONE)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_U32(m_inputVppType);

    std::vector<C2Config::profile_t> supported_profiles = {};
    std::vector<C2Config::level_t> supported_levels = {};

    MfxC2ParamStorage& pr = m_paramStorage;

    pr.RegisterParam<C2RateControlSetting>("RateControl");
    pr.RegisterParam<C2StreamFrameRateInfo::output>(C2_PARAMKEY_FRAME_RATE);
    pr.RegisterParam<C2StreamBitrateInfo::output>(C2_PARAMKEY_BITRATE);
    pr.RegisterParam<C2BitrateTuning::output>(MFX_C2_PARAMKEY_BITRATE_TUNING);

    pr.RegisterParam<C2FrameQPSetting>("FrameQP");
    const uint32_t MIN_QP = 1;
    const uint32_t MAX_QP = 51;
    pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_i, MIN_QP, MAX_QP);
    pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_p, MIN_QP, MAX_QP);
    pr.RegisterSupportedRange<C2FrameQPSetting>(&C2FrameQPSetting::qp_b, MIN_QP, MAX_QP);

    pr.RegisterParam<C2IntraRefreshTuning>("IntraRefresh");
    pr.RegisterSupportedRange<C2IntraRefreshTuning>(&C2IntraRefreshTuning::value, (int)false, (int)true);

    pr.RegisterParam<C2ProfileSetting>("Profile");
    pr.RegisterParam<C2LevelSetting>("Level");
    pr.RegisterParam<C2ProfileLevelInfo::output>("SupportedProfilesLevels");

    pr.RegisterParam<C2StreamInitDataInfo::output>(C2_PARAMKEY_INIT_DATA);
    pr.RegisterParam<C2MemoryTypeSetting>("MemoryType");

    pr.RegisterParam<C2StreamGopTuning::output>(C2_PARAMKEY_GOP);

    const unsigned int SINGLE_STREAM_ID = 0u;

    switch(m_encoderType) {
        case ENCODER_H264: {
            supported_profiles = {
                PROFILE_AVC_CONSTRAINED_BASELINE,
                PROFILE_AVC_BASELINE,
                PROFILE_AVC_MAIN,
                PROFILE_AVC_CONSTRAINED_HIGH,
                PROFILE_AVC_PROGRESSIVE_HIGH,
                PROFILE_AVC_HIGH,
            };
            supported_levels = {
                LEVEL_AVC_1, LEVEL_AVC_1B, LEVEL_AVC_1_1,
                LEVEL_AVC_1_2, LEVEL_AVC_1_3,
                LEVEL_AVC_2, LEVEL_AVC_2_1, LEVEL_AVC_2_2,
                LEVEL_AVC_3, LEVEL_AVC_3_1, LEVEL_AVC_3_2,
                LEVEL_AVC_4, LEVEL_AVC_4_1, LEVEL_AVC_4_2,
                LEVEL_AVC_5, LEVEL_AVC_5_1, LEVEL_AVC_5_2,
            };
            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::output>(SINGLE_STREAM_ID, PROFILE_AVC_CONSTRAINED_BASELINE, LEVEL_AVC_5_2));
            break;
        }
        case ENCODER_H265: {
            supported_profiles = {
                PROFILE_HEVC_MAIN,
                PROFILE_HEVC_MAIN_STILL,
                PROFILE_HEVC_MAIN_10,
            };

            supported_levels = {
                LEVEL_HEVC_MAIN_1,
                LEVEL_HEVC_MAIN_2, LEVEL_HEVC_MAIN_2_1,
                LEVEL_HEVC_MAIN_3, LEVEL_HEVC_MAIN_3_1,
                LEVEL_HEVC_MAIN_4, LEVEL_HEVC_MAIN_4_1,
                LEVEL_HEVC_MAIN_5, LEVEL_HEVC_MAIN_5_1,
                LEVEL_HEVC_MAIN_5_2, LEVEL_HEVC_HIGH_4,
                LEVEL_HEVC_HIGH_4_1, LEVEL_HEVC_HIGH_5,
                LEVEL_HEVC_HIGH_5_1, LEVEL_HEVC_HIGH_5_2,
            };
            pr.AddValue(C2_PARAMKEY_PROFILE_LEVEL,
                std::make_unique<C2StreamProfileLevelInfo::output>(SINGLE_STREAM_ID, PROFILE_HEVC_MAIN, LEVEL_HEVC_MAIN_5_1));
            break;
        }
    }

    pr.AddValue(C2_PARAMKEY_COMPONENT_DOMAIN,
        std::make_unique<C2ComponentDomainSetting>(C2Component::DOMAIN_VIDEO));

    pr.AddValue(C2_PARAMKEY_COMPONENT_KIND,
        std::make_unique<C2ComponentKindSetting>(C2Component::KIND_ENCODER));

    pr.AddValue(C2_PARAMKEY_INPUT_STREAM_BUFFER_TYPE,
        std::make_unique<C2StreamBufferTypeSetting::input>(SINGLE_STREAM_ID, C2BufferData::GRAPHIC));
    pr.AddValue(C2_PARAMKEY_OUTPUT_STREAM_BUFFER_TYPE,
        std::make_unique<C2StreamBufferTypeSetting::output>(SINGLE_STREAM_ID, C2BufferData::LINEAR));

    pr.AddValue(C2_PARAMKEY_INPUT_MEDIA_TYPE,
        AllocUniqueString<C2PortMediaTypeSetting::input>("video/raw"));

    if (m_encoderType == ENCODER_H264) {
        pr.AddValue(C2_PARAMKEY_OUTPUT_MEDIA_TYPE,
            AllocUniqueString<C2PortMediaTypeSetting::output>("video/avc"));
    } else {
        pr.AddValue(C2_PARAMKEY_OUTPUT_MEDIA_TYPE,
            AllocUniqueString<C2PortMediaTypeSetting::output>("video/hevc"));
    }

    pr.AddValue(C2_PARAMKEY_BITRATE_MODE,
        std::make_unique<C2StreamBitrateModeTuning::output>(SINGLE_STREAM_ID, C2Config::bitrate_mode_t::BITRATE_CONST));

    pr.AddStreamInfo<C2StreamPictureSizeInfo::input>(
        C2_PARAMKEY_PICTURE_SIZE, SINGLE_STREAM_ID,
        [this] (C2StreamPictureSizeInfo::input* dst)->bool {
            MFX_DEBUG_TRACE("GetPictureSize");
            dst->width = m_mfxVideoParamsConfig.mfx.FrameInfo.Width;
            dst->height = m_mfxVideoParamsConfig.mfx.FrameInfo.Height;
            MFX_DEBUG_TRACE_STREAM(NAMED(dst->width) << NAMED(dst->height));
            return true;
        },
        [this] (const C2StreamPictureSizeInfo::input& src)->bool {
            MFX_DEBUG_TRACE("SetPictureSize");
            m_mfxVideoParamsConfig.mfx.FrameInfo.Width = MFX_MEM_ALIGN(src.width, 16);
            m_mfxVideoParamsConfig.mfx.FrameInfo.Height = MFX_MEM_ALIGN(src.height, 16);
            m_mfxVideoParamsConfig.mfx.FrameInfo.CropW = MFX_MEM_ALIGN(src.width, 16);
            m_mfxVideoParamsConfig.mfx.FrameInfo.CropH = MFX_MEM_ALIGN(src.height, 16);
            MFX_DEBUG_TRACE_STREAM(NAMED(src.width) << NAMED(src.height));
            return true;
        }
    );

    // List all the supported profiles and levels
    pr.RegisterSupportedValues<C2StreamProfileLevelInfo>(&C2StreamProfileLevelInfo::C2ProfileLevelStruct::profile, supported_profiles);
    pr.RegisterSupportedValues<C2StreamProfileLevelInfo>(&C2StreamProfileLevelInfo::C2ProfileLevelStruct::level, supported_levels);

    m_paramStorage.DumpParams();
}

MfxC2EncoderComponent::~MfxC2EncoderComponent()
{
    MFX_DEBUG_TRACE_FUNC;

    Release();
}

void MfxC2EncoderComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;

    registry.RegisterMfxC2Component("c2.intel.avc.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H264>);
    registry.RegisterMfxC2Component("c2.intel.hevc.encoder",
        &MfxC2Component::Factory<MfxC2EncoderComponent, EncoderType>::Create<ENCODER_H265>);
}

c2_status_t MfxC2EncoderComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MfxDev::Create(MfxDev::Usage::Encoder, &m_device);

    if(mfx_res == MFX_ERR_NONE) mfx_res = ResetSettings(); // requires device_ initialized

    if(mfx_res == MFX_ERR_NONE) mfx_res = InitSession();

    return MfxStatusToC2(mfx_res);
}

c2_status_t MfxC2EncoderComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    m_uSyncedPointsCount = 0;
    mfxStatus mfx_res = MFX_ERR_NONE;
    m_bHeaderSent = false;

    do {
        bool allocator_required = (m_mfxVideoParamsConfig.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY);

        if (allocator_required != m_bAllocatorSet) {

#ifdef USE_ONEVPL
            mfx_res = MFXClose(m_mfxSession);
#else
            mfx_res = m_mfxSession.Close();
#endif
            if (MFX_ERR_NONE != mfx_res) break;

            mfx_res = InitSession();
            if (MFX_ERR_NONE != mfx_res) break;

            // set frame allocator
            if (allocator_required) {

                std::shared_ptr<MfxFrameAllocator> allocator = m_device->GetFrameAllocator();
                if (!allocator) {
                    mfx_res = MFX_ERR_NOT_INITIALIZED;
                    break;
                }

#ifdef USE_ONEVPL
                mfx_res = MFXVideoCORE_SetFrameAllocator(m_mfxSession, &allocator->GetMfxAllocator());
#else
                mfx_res = m_mfxSession.SetFrameAllocator(&allocator->GetMfxAllocator());
#endif
                if (MFX_ERR_NONE != mfx_res) break;

            } else {
#ifdef USE_ONEVPL
                mfx_res = MFXVideoCORE_SetFrameAllocator(m_mfxSession, nullptr);
#else
                mfx_res = m_mfxSession.SetFrameAllocator(nullptr);
#endif
                if (MFX_ERR_NONE != mfx_res) break;
            }
            m_bAllocatorSet = allocator_required;
        }
        m_workingQueue.Start();
        m_waitingQueue.Start();

        if (m_createConfig.dump_output) {

            std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            std::ostringstream oss;
            std::tm local_tm;
            localtime_r(&now_c, &local_tm);

            oss << m_name << "-" << std::put_time(std::localtime(&now_c), "%Y%m%d%H%M%S") << ".bin";

            MFX_DEBUG_TRACE_STREAM("Encoder output dump is started to " <<
                MFX_C2_DUMP_DIR << "/" << MFX_C2_DUMP_OUTPUT_SUB_DIR << "/" <<
                oss.str());

            m_outputWriter = std::make_unique<BinaryWriter>(MFX_C2_DUMP_DIR,
                std::vector<std::string>({MFX_C2_DUMP_OUTPUT_SUB_DIR}), oss.str());
        }

    } while(false);

    return C2_OK;
}

c2_status_t MfxC2EncoderComponent::DoStop(bool abort)
{
    MFX_DEBUG_TRACE_FUNC;

    // Working queue should stop first otherwise race condition
    // is possible when waiting queue is stopped (first), but working
    // queue is pushing tasks into it (via EncodeFrameAsync). As a
    // result, such tasks will be processed after next start
    // which is bad as sync point becomes invalid after
    // encoder Close/Init.
    if (abort) {
        m_workingQueue.Abort();
        m_waitingQueue.Abort();
    } else {
        m_workingQueue.Stop();
        m_waitingQueue.Stop();
    }

    while (!m_pendingWorks.empty()) {
        // Other statuses cause libstagefright_ccodec fatal error
        NotifyWorkDone(std::move(m_pendingWorks.front()), C2_NOT_FOUND);
        m_pendingWorks.pop();
    }

    FreeEncoder();

    m_outputWriter.reset();

    return C2_OK;
}

c2_status_t MfxC2EncoderComponent::Release()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    mfxStatus sts = MFX_ERR_NONE;

    m_lockedFrames.clear();
    m_vpp.Close();

#ifdef USE_ONEVPL
    if (m_mfxSession) {
        sts = MFXClose(m_mfxSession);
        m_mfxSession = nullptr;
    }
#else
    sts = m_mfxSession.Close();
#endif

    if (MFX_ERR_NONE != sts) res = MfxStatusToC2(sts);

    if (m_device) {
        m_device->Close();
        if (MFX_ERR_NONE != sts) res = MfxStatusToC2(sts);

        m_device = nullptr;
    }

#ifdef USE_ONEVPL
    if (m_mfxLoader) {
        MFXUnload(m_mfxLoader);
        m_mfxLoader = nullptr;
    }
#endif

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

#ifdef USE_ONEVPL
mfxStatus MfxC2EncoderComponent::InitSession()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;
    mfxConfig cfg[2];
    mfxVariant cfgVal[2];

    m_mfxLoader = MFXLoad();
    if (nullptr == m_mfxLoader) {
        ALOGE("MFXLoad failed...is implementation in path?");
        return MFX_ERR_UNKNOWN;
    }

    /* Create configurations for implementation */
    cfg[0] = MFXCreateConfig(m_mfxLoader);
    if (!cfg[0]) {
        ALOGE("Failed to create a MFX configuration");
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    cfgVal[0].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[0].Data.U32 = (mfx_implementation_ == MFX_IMPL_SOFTWARE) ? MFX_IMPL_TYPE_SOFTWARE : MFX_IMPL_TYPE_HARDWARE;
    mfx_res = MFXSetConfigFilterProperty(cfg[0], (const mfxU8 *) "mfxImplDescription.Impl", cfgVal[0]);
    if (MFX_ERR_NONE != mfx_res) {
        ALOGE("Failed to add an additional MFX configuration (%d)", mfx_res);
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    cfg[1] = MFXCreateConfig(m_mfxLoader);
    if (!cfg[1]) {
        ALOGE("Failed to create a MFX configuration");
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    cfgVal[1].Type = MFX_VARIANT_TYPE_U32;
    cfgVal[1].Data.U32 = MFX_VERSION;
    mfx_res = MFXSetConfigFilterProperty(cfg[1], (const mfxU8 *) "mfxImplDescription.ApiVersion.Version", cfgVal[1]);
    if (MFX_ERR_NONE != mfx_res) {
        ALOGE("Failed to add an additional MFX configuration (%d)", mfx_res);
        MFXUnload(m_mfxLoader);
        return MFX_ERR_UNKNOWN;
    }

    while (1) {
        /* Enumerate all implementations */
        uint32_t idx = 0;
        mfxImplDescription *idesc;
        mfx_res = MFXEnumImplementations(m_mfxLoader, idx, MFX_IMPLCAPS_IMPLDESCSTRUCTURE, (mfxHDL *)&idesc);

        if (MFX_ERR_NOT_FOUND == mfx_res) {
            /* Failed to find an available implementation */
            break;
        }
        else if (MFX_ERR_NONE != mfx_res) {
            /*implementation found, but requested query format is not supported*/
            idx++;
            continue;
        }
        ALOGI("%s. ApiVersion: %d.%d. Implementation type: %s. AccelerationMode via: %d",
                 __func__, idesc->ApiVersion.Major, idesc->ApiVersion.Minor,
                (idesc->Impl == MFX_IMPL_TYPE_SOFTWARE) ? "SW" : "HW",
                idesc->AccelerationMode);

        mfx_res = MFXCreateSession(m_mfxLoader, idx, &m_mfxSession);

        MFXDispReleaseImplDescription(m_mfxLoader, idesc);

        if (MFX_ERR_NONE == mfx_res)
            break;

        idx++;
    }

    if (MFX_ERR_NONE != mfx_res) {
        if (!m_mfxLoader)
            MFXUnload(m_mfxLoader);

        ALOGE("Failed to create a MFX session (%d)", mfx_res);
        return mfx_res;
    }

    //mfx_res = device_->InitMfxSession((MFXVideoSession*)const_cast<mfxSession>(m_mfxSession));
    mfx_res = device_->InitMfxSession(m_mfxSession);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

#else
mfxStatus MfxC2EncoderComponent::InitSession()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    do {
        mfx_res = m_mfxSession.Init(m_mfxImplementation, &g_required_mfx_version);
        if (MFX_ERR_NONE != mfx_res) {
            MFX_DEBUG_TRACE_MSG("MFXVideoSession::Init failed");
            break;
        }
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Major);
        MFX_DEBUG_TRACE_I32(g_required_mfx_version.Minor);

        mfx_res = m_mfxSession.QueryIMPL(&m_mfxImplementation);
        if (MFX_ERR_NONE != mfx_res) break;
        MFX_DEBUG_TRACE_I32(m_mfxImplementation);

        mfx_res = m_device->InitMfxSession(&m_mfxSession);

    } while (false);

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}
#endif

mfxStatus MfxC2EncoderComponent::ResetSettings()
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

    switch (m_encoderType)
    {
    case ENCODER_H264:
        m_mfxVideoParamsConfig.mfx.CodecId = MFX_CODEC_AVC;
        break;
   case ENCODER_H265:
        m_mfxVideoParamsConfig.mfx.CodecId = MFX_CODEC_HEVC;
        break;
    default:
        MFX_DEBUG_TRACE_MSG("unhandled codec type: BUG in plug-ins registration");
        break;
    }

    mfx_res = mfx_set_defaults_mfxVideoParam_enc(&m_mfxVideoParamsConfig);

    if (m_device) {
        // default pattern: video memory if allocator available
        m_mfxVideoParamsConfig.IOPattern = m_device->GetFrameAllocator() ?
            MFX_IOPATTERN_IN_VIDEO_MEMORY : MFX_IOPATTERN_IN_SYSTEM_MEMORY;
    } else {
        mfx_res = MFX_ERR_NULL_PTR;
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2EncoderComponent::InitEncoder(const mfxFrameInfo& frame_info)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;

    std::lock_guard<std::mutex> lock(m_initEncoderMutex);

    m_mfxVideoParamsConfig.mfx.FrameInfo = frame_info;
    if (MFX_ERR_NONE == mfx_res) {
#ifdef USE_ONEVPL
        m_mfxEncoder.reset(MFX_NEW_NO_THROW(MFXVideoENCODE(m_mfxSession)));
#else
        m_mfxEncoder.reset(MFX_NEW_NO_THROW(MFXVideoENCODE(m_mfxSession)));
#endif
        if (nullptr == m_mfxEncoder) {
            mfx_res = MFX_ERR_MEMORY_ALLOC;
        }

        if (MFX_ERR_NONE == mfx_res) {

            MFX_DEBUG_TRACE_MSG("Encoder initializing...");
            MFX_DEBUG_TRACE__mfxVideoParam_enc(m_mfxVideoParamsConfig);

            mfx_res = m_mfxEncoder->Init(&m_mfxVideoParamsConfig);

            MFX_DEBUG_TRACE_MSG("Encoder initialized");
            MFX_DEBUG_TRACE__mfxStatus(mfx_res);

            if (MFX_WRN_PARTIAL_ACCELERATION == mfx_res) {
                MFX_DEBUG_TRACE_MSG("InitEncoder returns MFX_WRN_PARTIAL_ACCELERATION");
                mfx_res = MFX_ERR_NONE;
            }
        }

        if (MFX_ERR_NONE == mfx_res) {
            mfx_res = m_mfxEncoder->GetVideoParam(&m_mfxVideoParamsState);
            MFX_DEBUG_TRACE__mfxVideoParam_enc(m_mfxVideoParamsState);
        }

        if (MFX_ERR_NONE != mfx_res) {
            FreeEncoder();
        }
    }

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

mfxStatus MfxC2EncoderComponent::InitVPP(C2FrameData& buf_pack)
{
    MFX_DEBUG_TRACE_FUNC;
    mfxStatus mfx_res = MFX_ERR_NONE;
    c2_status_t res;
    const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

    std::unique_ptr<C2ConstGraphicBlock> c_graph_block;
    std::unique_ptr<const C2GraphicView> c2_graphic_view_;
    res = GetC2ConstGraphicBlock(buf_pack, &c_graph_block);
        if(C2_OK != res) return MFX_ERR_NONE;

    res = MapConstGraphicBlock(*c_graph_block, TIMEOUT_NS, &c2_graphic_view_);

    if(c2_graphic_view_->layout().type == C2PlanarLayout::TYPE_RGB) {
        // need color convert to YUV
        MfxC2VppWrappParam param;

#ifdef USE_ONEVPL
        param.session = m_mfxSession;
#else
        param.session = &m_mfxSession;
#endif
        param.frame_info = &m_mfxVideoParamsConfig.mfx.FrameInfo;
        param.frame_info->Width = c_graph_block->width();
        param.frame_info->Height = c_graph_block->height();
        param.frame_info->CropX = 0;
        param.frame_info->CropY = 0;
        param.frame_info->CropW = c_graph_block->width();
        param.frame_info->CropH = c_graph_block->height();
        param.frame_info->FourCC = MFX_FOURCC_RGB4;
        param.allocator = m_device->GetFrameAllocator();
        param.conversion = ARGB_TO_NV12;

        mfx_res = m_vpp.Init(&param);
        m_inputVppType = param.conversion;
    } else {

        uint32_t width, height, format, stride, igbp_slot, generation;
        uint64_t usage, igbp_id;
        android::_UnwrapNativeCodec2GrallocMetadata(c_graph_block->handle(), &width, &height, &format, &usage,
                                                &stride, &generation, &igbp_id, &igbp_slot);
        if (!igbp_id && !igbp_slot) {
            //No surface & BQ
            m_mfxVideoParamsConfig.IOPattern = MFX_IOPATTERN_IN_SYSTEM_MEMORY;
            ALOGI("%s, format = 0x%x. System memory is being used for encoding!", __func__, format);
        }

        m_inputVppType = CONVERT_NONE;
    }

    if (MFX_ERR_NONE == mfx_res) m_bVppDetermined = true;

    MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    return mfx_res;
}

void MfxC2EncoderComponent::FreeEncoder()
{
    MFX_DEBUG_TRACE_FUNC;

    if(nullptr != m_mfxEncoder) {
        m_mfxEncoder->Close();
        m_mfxEncoder = nullptr;
    }
}

void MfxC2EncoderComponent::RetainLockedFrame(MfxC2FrameIn&& input)
{
    MFX_DEBUG_TRACE_FUNC;

    if(input.GetMfxFrameSurface()->Data.Locked) {
        m_lockedFrames.emplace_back(std::move(input));
    }
}

mfxStatus MfxC2EncoderComponent::EncodeFrameAsync(
    mfxEncodeCtrl *ctrl, mfxFrameSurface1 *surface, mfxBitstream *bs,
    mfxSyncPoint *syncp)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus sts = MFX_ERR_NONE;

    int trying_count = 0;
    const int MAX_TRYING_COUNT = 200;
    const auto timeout = std::chrono::milliseconds(5);

    do {
      sts = m_mfxEncoder->EncodeFrameAsync(ctrl, surface, bs, syncp);
      ++trying_count;

      if (MFX_WRN_DEVICE_BUSY == sts) {

        if (trying_count >= MAX_TRYING_COUNT) {
            MFX_DEBUG_TRACE_MSG("Too many MFX_WRN_DEVICE_BUSY from EncodeFrameAsync");
            sts = MFX_ERR_DEVICE_FAILED;
            break;
        }

        MFX_DEBUG_TRACE_STREAM("received MFX_WRN_DEVICE_BUSY [trying_count = " << trying_count << ", m_uSyncedPointsCount = " <<
            m_uSyncedPointsCount.load() << "]");
        std::unique_lock<std::mutex> lock(m_devBusyMutex);
        m_devBusyCond.wait_for(lock, timeout, [this] {
            if (m_uSyncedPointsCount < m_mfxVideoParamsState.AsyncDepth) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                return true;
            }
            else {
                return false;
            }});
      }
    } while (MFX_WRN_DEVICE_BUSY == sts);

    MFX_DEBUG_TRACE__mfxStatus(sts);
    return sts;
}

c2_status_t MfxC2EncoderComponent::AllocateBitstream(const std::unique_ptr<C2Work>& work,
    MfxC2BitstreamOut* mfx_bitstream)
{
    // TODO: allocation pool is required here
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        MFX_DEBUG_TRACE_I32(m_mfxVideoParamsState.mfx.BufferSizeInKB);
        MFX_DEBUG_TRACE_I32(m_mfxVideoParamsState.mfx.BRCParamMultiplier);
        mfxU32 required_size = m_mfxVideoParamsState.mfx.BufferSizeInKB * 1000 * m_mfxVideoParamsState.mfx.BRCParamMultiplier;
        MFX_DEBUG_TRACE_I32(required_size);

        if(work->worklets.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple worklets");
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

        if(worklet->output.buffers.size() != 0) {
            MFX_DEBUG_TRACE_MSG("Caller is not supposed to allocate output");
            res = C2_BAD_VALUE;
            break;
        }

        C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        std::shared_ptr<C2LinearBlock> out_block;

        res = m_c2Allocator->fetchLinearBlock(required_size, mem_usage, &out_block);
        if(C2_OK != res) break;

        res = MfxC2BitstreamOut::Create(out_block, TIMEOUT_NS, mfx_bitstream);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2EncoderComponent::ApplyWorkTunings(C2Work& work)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {

        if (work.worklets.size() != 1) {
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2Worklet>& worklet = work.worklets.front();
        if (nullptr == worklet) {
            res = C2_BAD_VALUE;
            break;
        }

        if (worklet->tunings.size() != 0) {
            // need this temp vector as cannot init vector<smth const> in one step
            std::vector<C2Param*> temp;
            std::transform(worklet->tunings.begin(), worklet->tunings.end(), std::back_inserter(temp),
                [] (const std::unique_ptr<C2Tuning>& p) { return p.get(); } );

            std::vector<C2Param*> params(temp.begin(), temp.end());

            std::vector<std::unique_ptr<C2SettingResult>> failures;
            {
                // These parameters update comes with C2Work from work queue,
                // there is no guarantee that state is not changed meanwhile
                // in contrast to Config method protected with state mutex.
                // So AcquireStableStateLock is needed here.
                std::unique_lock<std::mutex> lock = AcquireStableStateLock(true);
                DoConfig(params, &failures, false);
            }
            for(auto& failure : failures) {
                worklet->failures.push_back(std::move(failure));
            }
        }
    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

void MfxC2EncoderComponent::DoWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    c2_status_t res = C2_OK;

    do {
        if (!m_c2Allocator) {
            res = GetCodec2BlockPool(C2BlockPool::BASIC_LINEAR,
                shared_from_this(), &m_c2Allocator);
            if (res != C2_OK) break;
        }

        C2FrameData& input = work->input;
        MfxC2FrameIn mfx_frame_in;

        if (!m_bVppDetermined) {
            mfxStatus mfx_sts = InitVPP(input);
            if(MFX_ERR_NONE != mfx_sts) {
                MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                res = MfxStatusToC2(mfx_sts);
                break;
            }
        }

        std::shared_ptr<MfxFrameConverter> frame_converter;
        if (m_mfxVideoParamsConfig.IOPattern == MFX_IOPATTERN_IN_VIDEO_MEMORY) {
            frame_converter = m_device->GetFrameConverter();
        }

        if (CONVERT_NONE != m_inputVppType) {
            std::unique_ptr<mfxFrameSurface1> unique_mfx_frame =
                    std::make_unique<mfxFrameSurface1>();
            mfxFrameSurface1* pSurfaceToEncode;
            std::unique_ptr<const C2GraphicView> c_graph_view;

            std::unique_ptr<C2ConstGraphicBlock> c_graph_block;
            res = GetC2ConstGraphicBlock(input, &c_graph_block);
            MapConstGraphicBlock(*c_graph_block, TIMEOUT_NS, &c_graph_view);

            mfxMemId mem_id = nullptr;
            bool decode_target = false;
            native_handle_t *grallocHandle = android::UnwrapNativeCodec2GrallocHandle(c_graph_block->handle());

            mfxStatus mfx_sts = frame_converter->ConvertGrallocToVa(grallocHandle,
                                         decode_target, &mem_id);
            if (MFX_ERR_NONE != mfx_sts) {
                res = MfxStatusToC2(mfx_sts);
                break;
            }

            InitMfxFrameHW(input.ordinal.timestamp.peeku(), input.ordinal.frameIndex.peeku(),
                mem_id, c_graph_block->width(), c_graph_block->height(), MFX_FOURCC_RGB4, m_mfxVideoParamsConfig.mfx.FrameInfo,
                unique_mfx_frame.get());

            m_vpp.ProcessFrameVpp(unique_mfx_frame.get(), &pSurfaceToEncode);
            res = mfx_frame_in.init(NULL, std::move(c_graph_view), input, pSurfaceToEncode);
        } else {
            res = mfx_frame_in.init(frame_converter, input, m_mfxVideoParamsConfig.mfx.FrameInfo, TIMEOUT_NS);
        }
        if(C2_OK != res) break;

        if(nullptr == m_mfxEncoder) {
            // get frame format and size for encoder init from the first frame
            // should be got from slot descriptor
            mfxStatus mfx_sts = InitEncoder(mfx_frame_in.GetMfxFrameSurface()->Info);
            if(MFX_ERR_NONE != mfx_sts) {
                MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
                res = MfxStatusToC2(mfx_sts);
                break;
            }
        }

        MfxC2BitstreamOut mfx_bitstream;
        res = AllocateBitstream(work, &mfx_bitstream);
        if(C2_OK != res) break;

        mfxSyncPoint sync_point;

        res = ApplyWorkTunings(*work);
        if(C2_OK != res) break;

        std::unique_ptr<mfxEncodeCtrl> encode_ctrl = m_encoderControl.AcquireEncodeCtrl();

        mfxStatus mfx_sts = EncodeFrameAsync(encode_ctrl.get(),
            mfx_frame_in.GetMfxFrameSurface(), mfx_bitstream.GetMfxBitstream(), &sync_point);

        if (MFX_WRN_INCOMPATIBLE_VIDEO_PARAM == mfx_sts) mfx_sts = MFX_ERR_NONE;

        if( (MFX_ERR_NONE != mfx_sts) && (MFX_ERR_MORE_DATA != mfx_sts) ) {
            MFX_DEBUG_TRACE__mfxStatus(mfx_sts);
            res = MfxStatusToC2(mfx_sts);
            break;
        }

        m_waitingQueue.Push( [ mfx_frame = std::move(mfx_frame_in), this ] () mutable {
            RetainLockedFrame(std::move(mfx_frame));
        } );

        m_pendingWorks.push(std::move(work));

        if(MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<C2Work> work = std::move(m_pendingWorks.front());

            m_pendingWorks.pop();

            m_waitingQueue.Push(
                [ work = std::move(work), ec = std::move(encode_ctrl), bs = std::move(mfx_bitstream), sync_point, this ] () mutable {
                WaitWork(std::move(work), std::move(ec), std::move(bs), sync_point);
            } );

            {
                std::unique_lock<std::mutex> lock(m_devBusyMutex);
                ++m_uSyncedPointsCount;
            }
        }

        if(MFX_ERR_MORE_DATA == mfx_sts) mfx_sts = MFX_ERR_NONE;

    } while(false); // fake loop to have a cleanup point there

    if(C2_OK != res) { // notify listener in case of failure only
        NotifyWorkDone(std::move(work), res);
    }
}

void MfxC2EncoderComponent::Drain(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    while (!m_pendingWorks.empty()) {

        MfxC2BitstreamOut mfx_bitstream;
        res = AllocateBitstream(m_pendingWorks.front(), &mfx_bitstream);
        if(C2_OK != res) break;

        mfxSyncPoint sync_point;

        std::unique_ptr<mfxEncodeCtrl> encode_ctrl = m_encoderControl.AcquireEncodeCtrl();

        mfxStatus mfx_sts = EncodeFrameAsync(encode_ctrl.get(),
            nullptr/*input surface*/, mfx_bitstream.GetMfxBitstream(), &sync_point);

        if (MFX_ERR_NONE == mfx_sts) {

            std::unique_ptr<C2Work> work = std::move(m_pendingWorks.front());

            m_pendingWorks.pop();

            m_waitingQueue.Push(
                [ work = std::move(work), ec = std::move(encode_ctrl), bs = std::move(mfx_bitstream), sync_point, this ] () mutable {
                WaitWork(std::move(work), std::move(ec), std::move(bs), sync_point);
            } );

            {
                std::unique_lock<std::mutex> lock(m_devBusyMutex);
                ++m_uSyncedPointsCount;
            }
        } else {
            // MFX_ERR_MORE_DATA is an error here too -
            // we are calling EncodeFrameAsync times exactly how many outputs should be fetched
            res = MfxStatusToC2(mfx_sts);
            break;
        }
    }

    // eos work, should be sent after last work returned
    if (work) {
        m_waitingQueue.Push([work = std::move(work), this]() mutable {

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

            worklet->output.flags = (C2FrameData::flags_t)(work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
            worklet->output.ordinal = work->input.ordinal;

            NotifyWorkDone(std::move(work), C2_OK);
        });
    }

    if(C2_OK != res) {
        while(!m_pendingWorks.empty()) {
            NotifyWorkDone(std::move(m_pendingWorks.front()), res);
            m_pendingWorks.pop();
        }
    }
}

void MfxC2EncoderComponent::ReturnEmptyWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    if (work->worklets.size() > 0) {
        std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
        worklet->output.flags = work->input.flags;
        worklet->output.ordinal = work->input.ordinal;
    }
    NotifyWorkDone(std::move(work), C2_OK);
}

void MfxC2EncoderComponent::WaitWork(std::unique_ptr<C2Work>&& work,
    std::unique_ptr<mfxEncodeCtrl>&& encode_ctrl,
    MfxC2BitstreamOut&& bit_stream, mfxSyncPoint sync_point)
{
    MFX_DEBUG_TRACE_FUNC;

    mfxStatus mfx_res = MFX_ERR_NONE;

#ifdef USE_ONEVPL
    mfx_res = MFXVideoCORE_SyncOperation(m_mfxSession, sync_point, MFX_TIMEOUT_INFINITE);
#else
    mfx_res = m_mfxSession.SyncOperation(sync_point, MFX_TIMEOUT_INFINITE);
#endif

    if (MFX_ERR_NONE != mfx_res) {
        MFX_DEBUG_TRACE_MSG("SyncOperation failed");
        MFX_DEBUG_TRACE__mfxStatus(mfx_res);
    }

    // checking for unlocked surfaces and releasing them
    m_lockedFrames.remove_if(
        [] (const MfxC2FrameIn& mfx_frame)->bool { return !mfx_frame.GetMfxFrameSurface()->Data.Locked; } );

    // release encode_ctrl
    encode_ctrl = nullptr;

    if(MFX_ERR_NONE == mfx_res) {

        //C2Event event; // not supported yet, left for future use
        //event.fire(); // pre-fire event as output buffer is ready to use

        mfxBitstream* mfx_bitstream = bit_stream.GetMfxBitstream();
        MFX_DEBUG_TRACE_P(mfx_bitstream);

        if(!mfx_bitstream) mfx_res = MFX_ERR_NULL_PTR;
        else {
            MFX_DEBUG_TRACE_STREAM(NAMED(mfx_bitstream->DataOffset) << NAMED(mfx_bitstream->DataLength));

            if (m_outputWriter && mfx_bitstream->DataLength > 0) {
                m_outputWriter->Write(mfx_bitstream->Data + mfx_bitstream->DataOffset,
                    mfx_bitstream->DataLength);
            }

            C2ConstLinearBlock const_linear = bit_stream.GetC2LinearBlock()->share(
                mfx_bitstream->DataOffset,
                mfx_bitstream->DataLength, C2Fence()/*event.fence()*/);
            C2Buffer out_buffer = MakeC2Buffer( { const_linear } );
            if ((mfx_bitstream->FrameType & MFX_FRAMETYPE_IDR) != 0) {
                out_buffer.setInfo(std::make_shared<C2StreamPictureTypeMaskInfo::output>(0u/*stream id*/, C2Config::SYNC_FRAME));
            }

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();

            worklet->output.flags = work->input.flags;
            if (!m_bHeaderSent) {

                mfxExtCodingOptionSPSPPS* spspps{};
                mfxExtCodingOptionVPS* vps{};
                MfxVideoParamsWrapper video_param {};

                mfxU8 buf[256/*VPS*/ + 1024/*SPS*/ + 128/*PPS*/] = {0};
                try {
                    spspps = video_param.AddExtBuffer<mfxExtCodingOptionSPSPPS>();

                    if (ENCODER_H265 == m_encoderType)
                        vps = video_param.AddExtBuffer<mfxExtCodingOptionVPS>();

                    spspps->SPSBuffer = buf;
                    spspps->SPSBufSize = 1024;

                    spspps->PPSBuffer = buf + spspps->SPSBufSize;
                    spspps->PPSBufSize = 128;

                    if (ENCODER_H265 == m_encoderType) {
                        vps->VPSBuffer = spspps->PPSBuffer + spspps->PPSBufSize;
                        vps->VPSBufSize = 256;
                    }

                    mfx_res = m_mfxEncoder->GetVideoParam(&video_param);
                } catch(std::exception err) {
                    MFX_DEBUG_TRACE_STREAM("Error:" << err.what());
                    mfx_res = MFX_ERR_MEMORY_ALLOC;
                }

                if (MFX_ERR_NONE == mfx_res) {

                    int header_size = spspps->SPSBufSize + spspps->PPSBufSize;
                    if (ENCODER_H265 == m_encoderType)
                        header_size += vps->VPSBufSize;

                    std::unique_ptr<C2StreamInitDataInfo::output> csd =
                        C2StreamInitDataInfo::output::AllocUnique(header_size, 0u);

                    if (ENCODER_H265 == m_encoderType)
                        MFX_DEBUG_TRACE_STREAM("VPS: " << FormatHex(vps->VPSBuffer, vps->VPSBufSize));

                    MFX_DEBUG_TRACE_STREAM("SPS: " << FormatHex(spspps->SPSBuffer, spspps->SPSBufSize));
                    MFX_DEBUG_TRACE_STREAM("PPS: " << FormatHex(spspps->PPSBuffer, spspps->PPSBufSize));

                    uint8_t* dst = csd->m.value;

                    // Copy buffers in the order of VPS, SPS, PPS for HEVC or SPS, PPS for AVC
                    if (ENCODER_H265 == m_encoderType) {
                        std::copy(vps->VPSBuffer, vps->VPSBuffer + vps->VPSBufSize, dst);
                        dst += vps->VPSBufSize;
                    }

                    std::copy(spspps->SPSBuffer, spspps->SPSBuffer + spspps->SPSBufSize, dst);
                    dst += spspps->SPSBufSize;

                    std::copy(spspps->PPSBuffer, spspps->PPSBuffer + spspps->PPSBufSize, dst);

                    work->worklets.front()->output.configUpdate.push_back(std::move(csd));

                    worklet->output.flags = (C2FrameData::flags_t)(worklet->output.flags |
                        C2FrameData::FLAG_CODEC_CONFIG);

                    m_bHeaderSent = true;
                }
            }
            worklet->output.ordinal = work->input.ordinal;

            worklet->output.buffers.push_back(std::make_shared<C2Buffer>(out_buffer));
        }
    }

    // By resetting bit_stream we dispose of the bitrstream mapping here.
    bit_stream = MfxC2BitstreamOut();
    NotifyWorkDone(std::move(work), MfxStatusToC2(mfx_res));

    {
      std::unique_lock<std::mutex> lock(m_devBusyMutex);
      --m_uSyncedPointsCount;
    }
    m_devBusyCond.notify_one();
}

std::unique_ptr<mfxVideoParam> MfxC2EncoderComponent::GetParamsView() const
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<mfxVideoParam> res = std::make_unique<mfxVideoParam>();
    mfxStatus sts = MFX_ERR_NONE;

    if(nullptr == m_mfxEncoder) {
        MfxVideoParamsWrapper* in_params = const_cast<MfxVideoParamsWrapper*>(&m_mfxVideoParamsConfig);

        res->mfx.CodecId = in_params->mfx.CodecId;

        MFX_DEBUG_TRACE__mfxVideoParam_enc((*in_params));
        sts = MFXVideoENCODE_Query(
#ifdef USE_ONEVPL
            m_mfxSession,
#else
            (mfxSession)*const_cast<MFXVideoSession*>(&m_mfxSession),
#endif
            in_params, res.get());
        MFX_DEBUG_TRACE__mfxVideoParam_enc((*res));
    } else {
        sts = m_mfxEncoder->GetVideoParam(res.get());
    }

    MFX_DEBUG_TRACE__mfxStatus(sts);
    if (MFX_ERR_NONE != sts) {
        res = nullptr;
    } else {
        MFX_DEBUG_TRACE__mfxVideoParam_enc((*res));
    }
    return res;
}

c2_status_t MfxC2EncoderComponent::QueryParam(const mfxVideoParam* src, C2Param::Index index, C2Param** dst) const
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    res = m_paramStorage.QueryParam(index, dst);
    if (C2_NOT_FOUND == res) {
        res = C2_OK; // suppress error as second pass to find param

        switch (index.typeIndex()) {
            case kParamIndexRateControl: {
                if (nullptr == *dst) {
                    *dst = new C2RateControlSetting();
                }
                C2RateControlSetting* rate_control = (C2RateControlSetting*)*dst;
                switch(src->mfx.RateControlMethod) {
                    case MFX_RATECONTROL_CBR: rate_control->value = C2RateControlCBR; break;
                    case MFX_RATECONTROL_VBR: rate_control->value = C2RateControlVBR; break;
                    case MFX_RATECONTROL_CQP: rate_control->value = C2RateControlCQP; break;
                    default:
                        res = C2_CORRUPTED;
                        break;
                }
                break;
            }
            case kParamIndexFrameRate: {
                if (nullptr == *dst) {
                    *dst = new C2StreamFrameRateInfo::output();
                }
                C2StreamFrameRateInfo* framerate = (C2StreamFrameRateInfo*)*dst;
                framerate->value = (float)src->mfx.FrameInfo.FrameRateExtN / src->mfx.FrameInfo.FrameRateExtD;
                break;
            }
            case kParamIndexBitrate: {
                if (nullptr == *dst) {
                    *dst = new C2StreamBitrateInfo::output();
                }
                C2StreamBitrateInfo* bitrate = (C2StreamBitrateInfo*)*dst;
                if (src->mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                    bitrate->value = src->mfx.TargetKbps * 1000; // Convert from Kbps to bps
                } else {
                    res = C2_CORRUPTED;
                }
                break;
            }
            case kParamIndexFrameQP: {
                if (nullptr == *dst) {
                    *dst = new C2FrameQPSetting();
                }
                C2FrameQPSetting* frame_qp = (C2FrameQPSetting*)*dst;
                if (src->mfx.RateControlMethod == MFX_RATECONTROL_CQP) {
                    frame_qp->qp_i = src->mfx.QPI;
                    frame_qp->qp_p = src->mfx.QPP;
                    frame_qp->qp_b = src->mfx.QPB;
                } else {
                    res = C2_CORRUPTED;
                }
                break;
            }
            case kParamIndexProfile: {
                if (nullptr == *dst) {
                    *dst = new C2ProfileSetting();
                }
                C2Config::profile_t profile {};
                bool set_res = false;
                switch (m_encoderType) {
                    case ENCODER_H264:
                        set_res = AvcProfileMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecProfile, &profile);
                        break;
                    case ENCODER_H265:
                        set_res = HevcProfileMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecProfile, &profile);
                        break;
                    default:
                        break;
                }
                if (set_res) {
                    C2ProfileSetting* setting = static_cast<C2ProfileSetting*>(*dst);
                    setting->value = profile;
                } else {
                    res = C2_CORRUPTED;
                }
                break;
            }
            case kParamIndexLevel: {
                if (nullptr == *dst) {
                    *dst = new C2LevelSetting();
                }
                C2Config::level_t level {};
                bool set_res = false;
                switch (m_encoderType) {
                    case ENCODER_H264:
                        set_res = AvcLevelMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecLevel, &level);
                        break;
                    case ENCODER_H265:
                        set_res = HevcLevelMfxToAndroid(m_mfxVideoParamsConfig.mfx.CodecLevel, &level);
                        break;
                    default:
                        break;
                }
                if (set_res) {
                    C2LevelSetting* setting = static_cast<C2LevelSetting*>(*dst);
                    setting->value = level;
                } else {
                    res = C2_CORRUPTED;
                }
                break;
            }
            case kParamIndexProfileLevel:
                if (nullptr == *dst) {
                    if (m_encoderType == ENCODER_H264) {
                        std::unique_ptr<C2ProfileLevelInfo::output> info =
                            C2ProfileLevelInfo::output::AllocUnique(g_h264_profile_levels_count);

                        for (size_t i = 0; i < g_h264_profile_levels_count; ++i) {
                            info->m.values[i] = g_h264_profile_levels[i];
                        }
                        *dst = info.release();
                    } else if (m_encoderType == ENCODER_H265) {
                        std::unique_ptr<C2ProfileLevelInfo::output> info =
                            C2ProfileLevelInfo::output::AllocUnique(g_h265_profile_levels_count);

                        for (size_t i = 0; i < g_h265_profile_levels_count; ++i) {
                            info->m.values[i] = g_h265_profile_levels[i];
                        }
                        *dst = info.release();
                    } else {
                        res = C2_CORRUPTED;
                    }
                } else { // not possible to return flexible params through stack
                    res = C2_NO_MEMORY;
                }
                break;
            case kParamIndexMemoryType: {
                if (nullptr == *dst) {
                    *dst = new C2MemoryTypeSetting();
                }

                C2MemoryTypeSetting* setting = static_cast<C2MemoryTypeSetting*>(*dst);
                if (!MfxIOPatternToC2MemoryType(true, m_mfxVideoParamsConfig.IOPattern, &setting->value)) res = C2_CORRUPTED;
                break;
            }
            default:
                res = C2_BAD_INDEX;
                break;
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2EncoderComponent::Query(
    std::unique_lock<std::mutex> m_statelock,
    const std::vector<C2Param*> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    (void)m_statelock;
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> lock(m_initEncoderMutex);

    c2_status_t res = C2_OK;

    // determine source, update it if needed
    std::unique_ptr<mfxVideoParam> params_view = GetParamsView();
    if (nullptr != params_view) {
        // 1st cycle on stack params
        for (C2Param* param : stackParams) {
            c2_status_t param_res = C2_OK;
            if (m_paramStorage.FindParam(param->index())) {
                param_res = QueryParam(params_view.get(), param->index(), &param);
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
            c2_status_t param_res = C2_OK;
            if (m_paramStorage.FindParam(param_index.type())) {
                param_res = QueryParam(params_view.get(), param_index, &param);
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

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

void MfxC2EncoderComponent::DoConfig(const std::vector<C2Param*> &params,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures,
    bool queue_update)
{
    MFX_DEBUG_TRACE_FUNC;

    for (const C2Param* param : params) {
        // check whether plugin supports this parameter
        std::unique_ptr<C2SettingResult> find_res = m_paramStorage.FindParam(param);
        if(nullptr != find_res) {
            failures->push_back(std::move(find_res));
            continue;
        }
        // check whether plugin is in a correct state to apply this parameter
        // the check is bypassed for bitrate parameter as it should be updatable
        // despite its type 'info' (workaround of Google's bug)
        bool modifiable = (param->coreIndex().coreIndex() == kParamIndexBitrate) ||
            (param->kind() == C2Param::TUNING) || (param->kind() == C2Param::INFO) ||
            (param->kind() == C2Param::SETTING && m_state == State::STOPPED);

        if (!modifiable) {
            failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::READ_ONLY));
            continue;
        }

        // check ranges
        if(!m_paramStorage.ValidateParam(param, failures)) {
            continue;
        }

        // applying parameter
        switch (C2Param::Type(param->type()).typeIndex()) {
            case kParamIndexRateControl: {
                mfxStatus sts = MFX_ERR_NONE;
                switch (static_cast<const C2RateControlSetting*>(param)->value) {
                    case C2RateControlCBR:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, &m_mfxVideoParamsConfig);
                        break;
                    case C2RateControlVBR:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_VBR, &m_mfxVideoParamsConfig);
                        break;
                    case C2RateControlCQP:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CQP, &m_mfxVideoParamsConfig);
                        break;
                    default:
                        sts = MFX_ERR_INVALID_VIDEO_PARAM;
                        break;
                }
                if(MFX_ERR_NONE != sts) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexFrameRate: {
                float framerate_value = static_cast<const C2StreamFrameRateInfo*>(param)->value;

                if (m_encoderType == ENCODER_H264 && framerate_value > MFX_MAX_H264_FRAMERATE) {
                    framerate_value = MFX_MAX_H264_FRAMERATE;
                }
                else if (m_encoderType == ENCODER_H265 && framerate_value > MFX_MAX_H265_FRAMERATE) {
                    framerate_value = MFX_MAX_H265_FRAMERATE;
                }
                else {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE));
                    break;
                }

                m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtN = uint64_t(framerate_value * 1000); // keep 3 sign after dot
                m_mfxVideoParamsConfig.mfx.FrameInfo.FrameRateExtD = 1000;
                break;
            }
            case kParamIndexBitrate:
                if (m_mfxVideoParamsConfig.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                    uint32_t bitrate_value = static_cast<const C2BitrateTuning*>(param)->value;
                    if (m_state == State::STOPPED) {
                        m_mfxVideoParamsConfig.mfx.TargetKbps = bitrate_value / 1000; // Convert from bps to Kbps
                    } else if (m_mfxVideoParamsConfig.mfx.RateControlMethod == MFX_RATECONTROL_VBR) {
                        auto update_bitrate_value = [this, bitrate_value, queue_update, failures, param] () {
                            MFX_DEBUG_TRACE("update_bitrate_value");
                            MFX_DEBUG_TRACE_I32(bitrate_value);
                            m_mfxVideoParamsConfig.mfx.TargetKbps = bitrate_value / 1000; // Convert from bps to Kbps
                            if (nullptr != m_mfxEncoder) {
                                {   // waiting for encoding completion of all enqueued frames
                                    std::unique_lock<std::mutex> lock(m_devBusyMutex);
                                    // set big enough value to not hang if something unexpected happens
                                    const auto timeout = std::chrono::seconds(1);
                                    bool wait_res = m_devBusyCond.wait_for(lock, timeout, [this] { return m_uSyncedPointsCount == 0; } );
                                    if (!wait_res) {
                                        MFX_DEBUG_TRACE_MSG("WRN: Some encoded frames might skip during tunings change.");
                                    }
                                }
                                mfxStatus reset_sts = m_mfxEncoder->Reset(&m_mfxVideoParamsConfig);
                                MFX_DEBUG_TRACE__mfxStatus(reset_sts);
                                if (MFX_ERR_NONE != reset_sts) {
                                    if (!queue_update) {
                                        failures->push_back(MakeC2SettingResult(C2ParamField(param),
                                            C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                                    }
                                }
                            }
                        };

                        Drain(nullptr);

                        if (queue_update) {
                            m_workingQueue.Push(std::move(update_bitrate_value));
                        } else {
                            update_bitrate_value();
                        }
                    } else {
                        // If state is executing and rate control is not VBR, Reset will not update bitrate,
                        // so report an error.
                        failures->push_back(MakeC2SettingResult(C2ParamField(param),
                            C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                    }
                } else {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param),
                        C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                }
                break;
            case kParamIndexBitrateMode: {
                C2Config::bitrate_mode_t c2_bitmode = static_cast<const C2StreamBitrateModeTuning*>(param)->value;
                int32_t bitrate_mode = -1;
                C2Mapper::map(c2_bitmode, &bitrate_mode);
                MFX_DEBUG_TRACE_U32(bitrate_mode);
                mfxStatus sts = MFX_ERR_NONE;
                switch (bitrate_mode) {
                    case C2RateControlCBR:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CBR, &m_mfxVideoParamsConfig);
                        break;
                    case C2RateControlVBR:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_VBR, &m_mfxVideoParamsConfig);
                        break;
                    case C2RateControlCQP:
                        sts = mfx_set_RateControlMethod(MFX_RATECONTROL_CQP, &m_mfxVideoParamsConfig);
                        break;
                    default:
                        sts = MFX_ERR_INVALID_VIDEO_PARAM;
                        break;
                }
                if(MFX_ERR_NONE != sts) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexFrameQP: {
                const C2FrameQPSetting* qp_setting = static_cast<const C2FrameQPSetting*>(param);
                    if(m_mfxVideoParamsConfig.mfx.RateControlMethod != MFX_RATECONTROL_CQP) {
                        failures->push_back(MakeC2SettingResult(C2ParamField(param),
                            C2SettingResult::CONFLICT, MakeVector(MakeC2ParamField<C2RateControlSetting>())));
                    } else {
                        MFX_DEBUG_TRACE_STREAM(
                            NAMED(qp_setting->qp_i) << NAMED(qp_setting->qp_p) << NAMED(qp_setting->qp_b));
                        m_mfxVideoParamsConfig.mfx.QPI = qp_setting->qp_i;
                        m_mfxVideoParamsConfig.mfx.QPP = qp_setting->qp_p;
                        m_mfxVideoParamsConfig.mfx.QPB = qp_setting->qp_b;
                    }
                break;
            }
            case kParamIndexIntraRefresh: {
                const C2IntraRefreshTuning* intra_refresh = static_cast<const C2IntraRefreshTuning*>(param);
                if (intra_refresh->value != 0) {

                    auto update = [this] () {
                        EncoderControl::ModifyFunction modify = [] (mfxEncodeCtrl* ctrl) {
                            ctrl->FrameType = MFX_FRAMETYPE_IDR | MFX_FRAMETYPE_I;
                        };
                        m_encoderControl.Modify(modify);
                    };

                    if (queue_update) {
                        m_workingQueue.Push(std::move(update));
                    } else {
                        update();
                    }
                }
                break;
            }
            case kParamIndexProfile: {
                const C2ProfileSetting* profile_setting = static_cast<const C2ProfileSetting*>(param);
                C2Config::profile_t profile = static_cast<C2Config::profile_t>(profile_setting->value);
                bool set_res = false;
                switch (m_encoderType) {
                    case ENCODER_H264:
                        set_res = AvcProfileAndroidToMfx(profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        break;
                    case ENCODER_H265:
                        set_res = HevcProfileAndroidToMfx(profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        break;
                    default:
                        break;
                }

                if (!set_res) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexLevel: {
                const C2LevelSetting* setting = static_cast<const C2LevelSetting*>(param);
                C2Config::level_t level = static_cast<C2Config::level_t>(setting->value);
                bool set_res = false;
                switch (m_encoderType) {
                    case ENCODER_H264:
                        set_res = AvcLevelAndroidToMfx(level, &m_mfxVideoParamsConfig.mfx.CodecLevel);
                        break;
                    case ENCODER_H265:
                        set_res = HevcLevelAndroidToMfx(level, &m_mfxVideoParamsConfig.mfx.CodecLevel);
                        break;
                    default:
                        break;
                }

                if (!set_res) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexProfileLevel: {
                const C2StreamProfileLevelInfo* info = static_cast<const C2StreamProfileLevelInfo*>(param);
                if(info == nullptr) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::MISMATCH));
                    break;
                }
                switch (m_encoderType) {
                    case ENCODER_H264:
                        AvcProfileAndroidToMfx(info->profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        AvcLevelAndroidToMfx(info->level, &m_mfxVideoParamsConfig.mfx.CodecLevel);
                        break;
                    case ENCODER_H265:
                        HevcProfileAndroidToMfx(info->profile, &m_mfxVideoParamsConfig.mfx.CodecProfile);
                        HevcLevelAndroidToMfx(info->level, &m_mfxVideoParamsConfig.mfx.CodecLevel);
                        break;
                    default:
                        break;
                }
                break;
            }
            case kParamIndexMemoryType: {
                const C2MemoryTypeSetting* setting = static_cast<const C2MemoryTypeSetting*>(param);
                bool set_res = C2MemoryTypeToMfxIOPattern(true, setting->value, &m_mfxVideoParamsConfig.IOPattern);
                if (!set_res) {
                    failures->push_back(MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_VALUE));
                }
                break;
            }
            case kParamIndexGop: {
                const C2StreamGopTuning* setting = static_cast<const C2StreamGopTuning*>(param);
                m_mfxVideoParamsConfig.mfx.GopPicSize = setting->m.values[0].count;
                MFX_DEBUG_TRACE_I32(m_mfxVideoParamsConfig.mfx.GopPicSize);
                break;
            }
            default:
                m_paramStorage.ConfigParam(*param, m_state == State::STOPPED, failures);
                break;
        }
    }
}

c2_status_t MfxC2EncoderComponent::Config(std::unique_lock<std::mutex> m_statelock,
    const std::vector<C2Param*> &params,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures) {

    (void)m_statelock;
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if (nullptr == failures) {
            res = C2_CORRUPTED; break;
        }

        failures->clear();

        std::lock_guard<std::mutex> lock(m_initEncoderMutex);

        DoConfig(params, failures, true);

        res = GetAggregateStatus(failures);

    } while(false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2EncoderComponent::Queue(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& work : *items) {

        bool eos = (work->input.flags & C2FrameData::FLAG_END_OF_STREAM);
        bool empty = (work->input.buffers.size() == 0);
        MFX_DEBUG_TRACE_STREAM(NAMED(eos) << NAMED(empty));

        if (empty) {
            if (eos) {
                m_workingQueue.Push( [work = std::move(work), this] () mutable {
                    Drain(std::move(work));
                });
            } else {
                MFX_DEBUG_TRACE_MSG("Empty work without EOS flag, return back.");
                ReturnEmptyWork(std::move(work));
            }
        } else {
            m_workingQueue.Push( [ work = std::move(work), this ] () mutable {
                DoWork(std::move(work));
            } );

            if(eos || !m_pendingWorks.empty()) {
                m_workingQueue.Push( [this] () { Drain(nullptr); } );
            }
        }
    }

    return C2_OK;
}
