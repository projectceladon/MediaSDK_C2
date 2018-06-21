/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"

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
        res = acq_graph_view.get().error();
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
        res = acq_graph_view.get().error();
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
        res = acq_read_view.get().error();
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
        res = acq_write_view.get().error();
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
    std::vector<C2ParamFieldValues>&& conflicting_fields,
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
    C2Param::Index param_index)
{
    MFX_DEBUG_TRACE_FUNC;

    auto index_match = [param_index] (const auto& param_desc) {
        // C2Param::Index is most descriptive param ID, so its match means exact id match
        return param_index == param_desc->index();
    };

    bool res = std::any_of(params_desc.begin(), params_desc.end(), index_match);
    MFX_DEBUG_TRACE_I32(res);
    return res;
}

std::unique_ptr<C2SettingResult> FindC2Param(
    const std::vector<std::shared_ptr<C2ParamDescriptor>>& params_desc, const C2Param* param)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_STREAM(std::hex << NAMED(param->index()));

    std::unique_ptr<C2SettingResult> res;

    auto index_match = [param] (const auto& param_desc) {
        // C2Param::Index is most descriptive param ID, so its match means exact id match
        return param->index() == param_desc->index();
    };

    if (std::none_of(params_desc.begin(), params_desc.end(), index_match)) {
        // there is not exact match among supported parameters
        // if we find supported parameter with another port -> it is BAD_PORT error
        // otherwise -> BAD_TYPE error
        auto match_regardless_port = [param] (const auto& param_desc) {
            C2Param::Index indexA(param->index());
            C2Param::Index indexB(param_desc->index());

            return indexA.kind() == indexB.kind() &&
                indexA.forStream() == indexB.forStream() &&
                indexA.coreIndex() == indexB.coreIndex();
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

static const std::pair<C2Config::profile_t, mfxU16> g_h264_profiles[] =
{
    { PROFILE_AVC_BASELINE, MFX_PROFILE_AVC_CONSTRAINED_BASELINE },
    { PROFILE_AVC_MAIN, MFX_PROFILE_AVC_MAIN },
    { PROFILE_AVC_EXTENDED, MFX_PROFILE_AVC_EXTENDED },
    { PROFILE_AVC_HIGH, MFX_PROFILE_AVC_HIGH }
    /* PROFILE_AVC_HIGH_10, PROFILE_AVC_HIGH_422, PROFILE_AVC_HIGH_444
    are not supported */
};

static const std::pair<C2Config::level_t, mfxU16> g_h264_levels[] =
{
    { LEVEL_AVC_1,  MFX_LEVEL_AVC_1 },
    { LEVEL_AVC_1B, MFX_LEVEL_AVC_1b },
    { LEVEL_AVC_1_1, MFX_LEVEL_AVC_11 },
    { LEVEL_AVC_1_2, MFX_LEVEL_AVC_12 },
    { LEVEL_AVC_1_3, MFX_LEVEL_AVC_13 },
    { LEVEL_AVC_2,  MFX_LEVEL_AVC_2 },
    { LEVEL_AVC_2_1, MFX_LEVEL_AVC_21 },
    { LEVEL_AVC_2_2, MFX_LEVEL_AVC_22 },
    { LEVEL_AVC_3,  MFX_LEVEL_AVC_3 },
    { LEVEL_AVC_3_1, MFX_LEVEL_AVC_31 },
    { LEVEL_AVC_3_2, MFX_LEVEL_AVC_32 },
    { LEVEL_AVC_4,  MFX_LEVEL_AVC_4 },
    { LEVEL_AVC_4_1, MFX_LEVEL_AVC_41 },
    { LEVEL_AVC_4_2, MFX_LEVEL_AVC_42 },
    { LEVEL_AVC_5,  MFX_LEVEL_AVC_5 },
    { LEVEL_AVC_5_1, MFX_LEVEL_AVC_51 }
};

static const std::pair<C2Config::profile_t, mfxU16> g_h265_profiles[] =
{
    { PROFILE_HEVC_MAIN,  MFX_PROFILE_HEVC_MAIN },
    { PROFILE_HEVC_MAIN_10, MFX_PROFILE_HEVC_MAIN10 }
    /* PROFILE_HEVC_MAINSP, PROFILE_HEVC_REXT, PROFILE_HEVC_SCC
    are not supported */
};

static const std::pair<C2Config::level_t, mfxU16> g_h265_levels[] =
{
    { LEVEL_HEVC_MAIN_1, MFX_LEVEL_HEVC_1 },
    { LEVEL_HEVC_MAIN_2, MFX_LEVEL_HEVC_2},
    { LEVEL_HEVC_MAIN_2_1, MFX_LEVEL_HEVC_21 },
    { LEVEL_HEVC_MAIN_3, MFX_LEVEL_HEVC_3 },
    { LEVEL_HEVC_MAIN_3_1, MFX_LEVEL_HEVC_31 },
    { LEVEL_HEVC_MAIN_4, MFX_LEVEL_HEVC_4 },
    { LEVEL_HEVC_MAIN_4_1, MFX_LEVEL_HEVC_41 },
    { LEVEL_HEVC_MAIN_5, MFX_LEVEL_HEVC_5 },
    { LEVEL_HEVC_MAIN_5_1, MFX_LEVEL_HEVC_51 },
    { LEVEL_HEVC_MAIN_5_2, MFX_LEVEL_HEVC_52 },
    { LEVEL_HEVC_MAIN_6, MFX_LEVEL_HEVC_6 },
    { LEVEL_HEVC_MAIN_6_1, MFX_LEVEL_HEVC_61 },
    { LEVEL_HEVC_MAIN_6_2, MFX_LEVEL_HEVC_62 }
};

bool AvcProfileAndroidToMfx(C2Config::profile_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_h264_profiles, android_value, mfx_value);
}

bool AvcProfileMfxToAndroid(mfxU16 mfx_value, C2Config::profile_t* android_value)
{
    return SecondToFirst(g_h264_profiles, mfx_value, android_value);
}

bool AvcLevelAndroidToMfx(C2Config::level_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_h264_levels, android_value, mfx_value);
}

bool AvcLevelMfxToAndroid(mfxU16 mfx_value, C2Config::level_t* android_value)
{
    return SecondToFirst(g_h264_levels, mfx_value, android_value);
}

bool HevcProfileAndroidToMfx(C2Config::profile_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_h265_profiles, android_value, mfx_value);
}

bool HevcProfileMfxToAndroid(mfxU16 mfx_value, C2Config::profile_t* android_value)
{
    return SecondToFirst(g_h265_profiles, mfx_value, android_value);
}

bool HevcLevelAndroidToMfx(C2Config::level_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_h265_levels, android_value, mfx_value);
}

bool HevcLevelMfxToAndroid(mfxU16 mfx_value, C2Config::level_t* android_value)
{
    return SecondToFirst(g_h265_levels, mfx_value, android_value);
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
