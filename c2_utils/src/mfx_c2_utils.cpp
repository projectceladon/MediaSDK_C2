/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_legacy_defs.h"

using namespace android;

c2_status_t MfxStatusToC2(mfxStatus mfx_status)
{
    switch(mfx_status) {
        case MFX_ERR_NONE:
            return C2_OK;

        case MFX_ERR_NULL_PTR:
        case MFX_ERR_INVALID_HANDLE:
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        case MFX_ERR_INVALID_VIDEO_PARAM:
        case MFX_ERR_INCOMPATIBLE_AUDIO_PARAM:
        case MFX_ERR_INVALID_AUDIO_PARAM:
            return C2_BAD_VALUE;

        case MFX_ERR_UNSUPPORTED:
            return C2_CANNOT_DO;

        case MFX_ERR_NOT_FOUND:
            return C2_NOT_FOUND;

        case MFX_ERR_MORE_BITSTREAM:
        case MFX_ERR_MORE_DATA:
        case MFX_ERR_MORE_SURFACE:
        case MFX_ERR_NOT_INITIALIZED:
            return C2_BAD_STATE;

        case MFX_ERR_MEMORY_ALLOC:
        case MFX_ERR_NOT_ENOUGH_BUFFER:
        case MFX_ERR_LOCK_MEMORY:
            return C2_NO_MEMORY;

        case MFX_ERR_GPU_HANG:
            return C2_TIMED_OUT;

        case MFX_ERR_UNKNOWN:
        case MFX_ERR_UNDEFINED_BEHAVIOR:
        case MFX_ERR_DEVICE_FAILED:
        case MFX_ERR_ABORTED:
        case MFX_ERR_DEVICE_LOST:
        default:
            return C2_CORRUPTED;
    }
}

c2_status_t GetC2ConstGraphicBlock(
    const C2FrameData& buf_pack, std::unique_ptr<C2ConstGraphicBlock>* c_graph_block)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_BAD_VALUE;

    do {
        if(nullptr == c_graph_block) break;

        if(buf_pack.buffers.size() != 1) break;

        std::shared_ptr<C2Buffer> in_buffer = buf_pack.buffers.front();
        if(nullptr == in_buffer) break;

        const C2BufferData& in_buf_data = in_buffer->data();
        if(in_buf_data.type() != C2BufferData::GRAPHIC) break;

        (*c_graph_block) = std::make_unique<C2ConstGraphicBlock>(in_buf_data.graphicBlocks().front());

        res = C2_OK;

    } while(false);

    return res;
}

c2_status_t GetC2ConstLinearBlock(
    const C2FrameData& buf_pack, std::unique_ptr<C2ConstLinearBlock>* c_lin_block)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_BAD_VALUE;

    do {
        if(nullptr == c_lin_block) break;

        if(buf_pack.buffers.size() != 1) break;

        std::shared_ptr<C2Buffer> in_buffer = buf_pack.buffers.front();
        if(nullptr == in_buffer) break;

        const C2BufferData& in_buf_data = in_buffer->data();
        if(in_buf_data.type() != C2BufferData::LINEAR) break;

        (*c_lin_block) = std::make_unique<C2ConstLinearBlock>(in_buf_data.linearBlocks().front());

        res = C2_OK;

    } while(false);

    return res;
}

c2_status_t MapConstGraphicBlock(const C2ConstGraphicBlock& graph_block, c2_nsecs_t timeout,
    std::unique_ptr<const C2GraphicView>* graph_view)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if(nullptr == graph_view) {
            res = C2_BAD_VALUE;
            break;
        }

        C2Acquirable<const C2GraphicView> acq_graph_view = graph_block.map();
        res = acq_graph_view.GetError();
        if(C2_OK != res) break;

        res = acq_graph_view.wait(timeout);
        if(C2_OK != res) break;

        *graph_view = std::make_unique<C2GraphicView>(acq_graph_view.get());

    } while(false);

    return res;
}

c2_status_t MapGraphicBlock(C2GraphicBlock& graph_block, c2_nsecs_t timeout,
    std::unique_ptr<C2GraphicView>* graph_view)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if(nullptr == graph_view) {
            res = C2_BAD_VALUE;
            break;
        }

        C2Acquirable<C2GraphicView> acq_graph_view = graph_block.map();
        res = acq_graph_view.GetError();
        if(C2_OK != res) break;

        res = acq_graph_view.wait(timeout);
        if(C2_OK != res) break;

        *graph_view = std::make_unique<C2GraphicView>(acq_graph_view.get());

    } while(false);

    return res;
}

c2_status_t MapConstLinearBlock(const C2ConstLinearBlock& c_lin_block, c2_nsecs_t timeout,
    std::unique_ptr<C2ReadView>* read_view)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if(nullptr == read_view) {
            res = C2_BAD_VALUE;
            break;
        }

        res = c_lin_block.fence().wait(timeout);
        if(C2_OK != res) break;

        C2Acquirable<C2ReadView> acq_read_view = c_lin_block.map();
        res = acq_read_view.GetError();
        if(C2_OK != res) break;

        res = acq_read_view.wait(timeout);
        if(C2_OK != res) break;

        *read_view = std::make_unique<C2ReadView>(acq_read_view.get());

    } while(false);

    return res;
}

c2_status_t MapLinearBlock(C2LinearBlock& lin_block, c2_nsecs_t timeout,
    std::unique_ptr<C2WriteView>* write_view)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if(nullptr == write_view) {
            res = C2_BAD_VALUE;
            break;
        }

        C2Acquirable<C2WriteView> acq_write_view = lin_block.map();
        res = acq_write_view.GetError();
        if(C2_OK != res) break;

        res = acq_write_view.wait(timeout);
        if(C2_OK != res) break;

        *write_view = std::make_unique<C2WriteView>(acq_write_view.get());

    } while(false);

    return res;
}

std::unique_ptr<C2SettingResult> MakeC2SettingResult(
    const C2ParamField& param_field,
    C2SettingResult::Failure failure,
    std::list<C2ParamFieldValues>&& conflicting_fields,
    const C2FieldSupportedValues* supported_values)
{
    std::unique_ptr<C2FieldSupportedValues> supported_values_unique;
    if(nullptr != supported_values) {
        supported_values_unique =
            std::make_unique<C2FieldSupportedValues>(*supported_values);
    }

    C2SettingResult* set_res = new C2SettingResult { failure, { param_field,
        std::move(supported_values_unique) }, std::move(conflicting_fields) };
    return std::unique_ptr<C2SettingResult>(set_res);
}

c2_status_t GetAggregateStatus(std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    if (!failures->empty()) {

        auto is_bad_value = [] (const std::unique_ptr<C2SettingResult>& set_res) {
            return set_res->failure == C2SettingResult::BAD_VALUE;
        };

        if (std::all_of(failures->begin(), failures->end(), is_bad_value)) {
            res = C2_BAD_VALUE;
        } else { // if any other failures
            res = C2_BAD_INDEX;
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

bool FindC2Param(
    const std::vector<std::shared_ptr<C2ParamDescriptor>>& params_desc,
    C2Param::Type param_type)
{
    MFX_DEBUG_TRACE_FUNC;

    auto type_match = [param_type] (const auto& param_desc) {
        // type includes: kind, dir, flexible and param_index, doesn't include stream id
        // stream should be checked on individual parameters handling
        return param_type == param_desc->type();
    };

    bool res = std::any_of(params_desc.begin(), params_desc.end(), type_match);
    MFX_DEBUG_TRACE_I32(res);
    return res;
}

std::unique_ptr<C2SettingResult> FindC2Param(
    const std::vector<std::shared_ptr<C2ParamDescriptor>>& params_desc, const C2Param* param)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_STREAM(NAMED(param->index()));

    std::unique_ptr<C2SettingResult> res;

    auto type_match = [param] (const auto& param_desc) {
        // type includes: kind, dir, flexible and param_index, doesn't include stream id
        // stream should be checked on individual parameters handling
        return C2Param::Type(param->type()) == param_desc->type();
    };

    if (std::none_of(params_desc.begin(), params_desc.end(), type_match)) {
        // there is not exact match among supported parameters
        // if we find supported parameter with another port -> it is BAD_PORT error
        // otherwise -> BAD_TYPE error
        auto match_regardless_port = [param] (const auto& param_desc) {
            C2Param::Type typeA(param->type());
            C2Param::Type typeB(param_desc->type());

            return typeA.kind() == typeB.kind() &&
                typeA.forStream() == typeB.forStream() &&
                typeA.coreIndex() == typeB.coreIndex();
        };

        if (std::any_of(params_desc.begin(), params_desc.end(), match_regardless_port)) {
            res = MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_PORT);
        } else {
            res = MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE);
        }
    }
    MFX_DEBUG_TRACE_P(res.get());
    return res;
}

static const std::pair<LEGACY_VIDEO_AVCPROFILETYPE, mfxU16> g_h264_profiles[] =
{
    { LEGACY_VIDEO_AVCProfileBaseline, MFX_PROFILE_AVC_CONSTRAINED_BASELINE },
    { LEGACY_VIDEO_AVCProfileMain, MFX_PROFILE_AVC_MAIN },
    { LEGACY_VIDEO_AVCProfileExtended, MFX_PROFILE_AVC_EXTENDED },
    { LEGACY_VIDEO_AVCProfileHigh, MFX_PROFILE_AVC_HIGH }
    /* LEGACY_VIDEO_AVCProfileHigh10, LEGACY_VIDEO_AVCProfileHigh422, LEGACY_VIDEO_AVCProfileHigh444
    are not supported */
};

static const std::pair<LEGACY_VIDEO_AVCLEVELTYPE, mfxU16> g_h264_levels[] =
{
    { LEGACY_VIDEO_AVCLevel1,  MFX_LEVEL_AVC_1 },
    { LEGACY_VIDEO_AVCLevel1b, MFX_LEVEL_AVC_1b },
    { LEGACY_VIDEO_AVCLevel11, MFX_LEVEL_AVC_11 },
    { LEGACY_VIDEO_AVCLevel12, MFX_LEVEL_AVC_12 },
    { LEGACY_VIDEO_AVCLevel13, MFX_LEVEL_AVC_13 },
    { LEGACY_VIDEO_AVCLevel2,  MFX_LEVEL_AVC_2 },
    { LEGACY_VIDEO_AVCLevel21, MFX_LEVEL_AVC_21 },
    { LEGACY_VIDEO_AVCLevel22, MFX_LEVEL_AVC_22 },
    { LEGACY_VIDEO_AVCLevel3,  MFX_LEVEL_AVC_3 },
    { LEGACY_VIDEO_AVCLevel31, MFX_LEVEL_AVC_31 },
    { LEGACY_VIDEO_AVCLevel32, MFX_LEVEL_AVC_32 },
    { LEGACY_VIDEO_AVCLevel4,  MFX_LEVEL_AVC_4 },
    { LEGACY_VIDEO_AVCLevel41, MFX_LEVEL_AVC_41 },
    { LEGACY_VIDEO_AVCLevel42, MFX_LEVEL_AVC_42 },
    { LEGACY_VIDEO_AVCLevel5,  MFX_LEVEL_AVC_5 },
    { LEGACY_VIDEO_AVCLevel51, MFX_LEVEL_AVC_51 }
};

bool AvcProfileAndroidToMfx(uint32_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_h264_profiles,
        static_cast<LEGACY_VIDEO_AVCPROFILETYPE>(android_value), mfx_value);
}

bool AvcProfileMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value)
{
    return SecondToFirst(g_h264_profiles, mfx_value, android_value);
}

bool AvcLevelAndroidToMfx(uint32_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_h264_levels,
        static_cast<LEGACY_VIDEO_AVCLEVELTYPE>(android_value), mfx_value);
}

bool AvcLevelMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value)
{
    return SecondToFirst(g_h264_levels, mfx_value, android_value);
}

// Returns pointers to NV12 planes.
void InitNV12PlaneData(int32_t pitch, int32_t alloc_height, uint8_t* base, uint8_t** plane_data)
{
    plane_data[C2PlanarLayout::PLANE_Y] = base;
    plane_data[C2PlanarLayout::PLANE_U] = base + alloc_height * pitch;
    plane_data[C2PlanarLayout::PLANE_V] = base + alloc_height * pitch + 1;
}

void InitNV12PlaneLayout(int32_t pitch, C2PlanarLayout* layout)
{
    layout->type = C2PlanarLayout::TYPE_YUV;
    layout->numPlanes = 3;

    C2PlaneInfo& y_plane = layout->planes[C2PlanarLayout::PLANE_Y];
    y_plane.channel = C2PlaneInfo::CHANNEL_Y;
    y_plane.colInc = 1;
    y_plane.rowInc = pitch;
    y_plane.colSampling = 1;
    y_plane.rowSampling = 1;
    y_plane.bitDepth = 8;
    y_plane.allocatedDepth = 8;
    y_plane.rightShift = 0;
    y_plane.endianness = C2PlaneInfo::NATIVE;

    C2PlaneInfo& u_plane = layout->planes[C2PlanarLayout::PLANE_U];
    u_plane.channel = C2PlaneInfo::CHANNEL_CB;

    C2PlaneInfo& v_plane = layout->planes[C2PlanarLayout::PLANE_V];
    v_plane.channel = C2PlaneInfo::CHANNEL_CR;

    for (C2PlanarLayout::plane_index_t plane_index : { C2PlanarLayout::PLANE_U, C2PlanarLayout::PLANE_V }) {
        C2PlaneInfo& plane = layout->planes[plane_index];
        plane.colInc = 2;
        plane.rowInc = pitch;
        plane.colSampling = 2;
        plane.rowSampling = 2;
        plane.bitDepth = 8;
        plane.allocatedDepth = 8;
        plane.rightShift = 0;
        plane.endianness = C2PlaneInfo::NATIVE;
    }
}

static const std::pair<C2MemoryType, mfxU16> g_input_memory_types[] =
{
    { C2MemoryTypeSystem, MFX_IOPATTERN_IN_SYSTEM_MEMORY },
    { C2MemoryTypeGraphics, MFX_IOPATTERN_IN_VIDEO_MEMORY }
};

static const std::pair<C2MemoryType, mfxU16> g_output_memory_types[] =
{
    { C2MemoryTypeSystem, MFX_IOPATTERN_OUT_SYSTEM_MEMORY },
    { C2MemoryTypeGraphics, MFX_IOPATTERN_OUT_VIDEO_MEMORY }
};

bool C2MemoryTypeToMfxIOPattern(bool input, C2MemoryType memory_type, mfxU16* io_pattern)
{
    return FirstToSecond(input ? g_input_memory_types : g_output_memory_types,
        memory_type, io_pattern);
}

bool MfxIOPatternToC2MemoryType(bool input, mfxU16 io_pattern, C2MemoryType* memory_type)
{
    return SecondToFirst(input ? g_input_memory_types : g_output_memory_types,
        io_pattern, memory_type);
}

int MfxFourCCToGralloc(mfxU32 fourcc)
{
    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL;
        default:
            return 0;
    }
}
