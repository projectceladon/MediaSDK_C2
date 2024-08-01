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


#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"

#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_utils"

c2_status_t MfxStatusToC2(mfxStatus mfx_status)
{
    switch(mfx_status) {
        case MFX_ERR_NONE:
            return C2_OK;

        case MFX_ERR_NULL_PTR:
        case MFX_ERR_INVALID_HANDLE:
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        case MFX_ERR_INVALID_VIDEO_PARAM:
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

c2_status_t MapConstGraphicBlock(const C2ConstGraphicBlock& graph_block, c2_nsecs_t /*timeout*/,
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

        // res = acq_graph_view.wait(timeout); C2Fence::wait not supported yet
        // if(C2_OK != res) break;

        *graph_view = std::make_unique<C2GraphicView>(acq_graph_view.get());

    } while(false);

    return res;
}

c2_status_t MapGraphicBlock(C2GraphicBlock& graph_block, c2_nsecs_t /*timeout*/,
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

        // res = acq_graph_view.wait(timeout); C2Fence::wait not supported yet
        // if(C2_OK != res) break;

        *graph_view = std::make_unique<C2GraphicView>(acq_graph_view.get());

    } while(false);

    return res;
}

c2_status_t MapConstLinearBlock(const C2ConstLinearBlock& c_lin_block, c2_nsecs_t /*timeout*/,
    std::unique_ptr<C2ReadView>* read_view)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        if(nullptr == read_view) {
            res = C2_BAD_VALUE;
            break;
        }

        // res = c_lin_block.fence().wait(timeout); C2Fence::wait not supported yet
        // if(C2_OK != res) break;

        C2Acquirable<C2ReadView> acq_read_view = c_lin_block.map();
        res = acq_read_view.get().error();
        if(C2_OK != res) break;

        // res = acq_read_view.wait(timeout); C2Fence::wait not supported yet
        // if(C2_OK != res) break;

        *read_view = std::make_unique<C2ReadView>(acq_read_view.get());

    } while(false);

    return res;
}

c2_status_t MapLinearBlock(C2LinearBlock& lin_block, c2_nsecs_t /*timeout*/,
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

        // res = acq_write_view.wait(timeout); C2Fence::wait not supported yet
        // if(C2_OK != res) break;

        *write_view = std::make_unique<C2WriteView>(acq_write_view.get());

    } while(false);

    return res;
}

std::shared_ptr<C2Buffer> CreateGraphicBuffer(
    const std::shared_ptr<C2GraphicBlock> &block, const C2Rect &crop)
{
    return C2Buffer::CreateGraphicBuffer(block->share(crop, ::C2Fence()));
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
    MFX_DEBUG_TRACE_STREAM(std::hex << (uint32_t)param_index);

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
    { PROFILE_AVC_BASELINE, MFX_PROFILE_AVC_BASELINE },
    { PROFILE_AVC_CONSTRAINED_BASELINE, MFX_PROFILE_AVC_CONSTRAINED_BASELINE },
    { PROFILE_AVC_MAIN, MFX_PROFILE_AVC_MAIN },
    { PROFILE_AVC_CONSTRAINED_HIGH, MFX_PROFILE_AVC_CONSTRAINED_HIGH },
    { PROFILE_AVC_PROGRESSIVE_HIGH, MFX_PROFILE_AVC_PROGRESSIVE_HIGH },
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
    { PROFILE_HEVC_MAIN_STILL, MFX_PROFILE_HEVC_MAINSP },
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

static const std::pair<C2Config::profile_t, mfxU16> g_vp9_profiles[] =
{
    { PROFILE_VP9_0,  MFX_PROFILE_VP9_0 },
    { PROFILE_VP9_1, MFX_PROFILE_VP9_1 },
    { PROFILE_VP9_2, MFX_PROFILE_VP9_2 },
    { PROFILE_VP9_3, MFX_PROFILE_VP9_3 }
};

static const std::pair<C2Config::level_t, mfxU16> g_av1_levels[] =
{
    { LEVEL_AV1_2,  MFX_LEVEL_AV1_2 },
    { LEVEL_AV1_2_1, MFX_LEVEL_AV1_21 },
    { LEVEL_AV1_2_2, MFX_LEVEL_AV1_22 },
    { LEVEL_AV1_2_3, MFX_LEVEL_AV1_23 },
    { LEVEL_AV1_3, MFX_LEVEL_AV1_3 },
    { LEVEL_AV1_3_1,  MFX_LEVEL_AV1_31 },
    { LEVEL_AV1_3_2, MFX_LEVEL_AV1_32 },
    { LEVEL_AV1_3_3, MFX_LEVEL_AV1_33 },
    { LEVEL_AV1_4,  MFX_LEVEL_AV1_4 },
    { LEVEL_AV1_4_1, MFX_LEVEL_AV1_41 },
    { LEVEL_AV1_4_2, MFX_LEVEL_AV1_42 },
    { LEVEL_AV1_4_3,  MFX_LEVEL_AV1_43 },
    { LEVEL_AV1_5, MFX_LEVEL_AV1_5 },
    { LEVEL_AV1_5_1, MFX_LEVEL_AV1_51 },
    { LEVEL_AV1_5_2,  MFX_LEVEL_AV1_52 },
    { LEVEL_AV1_5_3, MFX_LEVEL_AV1_53 },
    { LEVEL_AV1_6, MFX_LEVEL_AV1_6 },
    { LEVEL_AV1_6_1,  MFX_LEVEL_AV1_61 },
    { LEVEL_AV1_6_2, MFX_LEVEL_AV1_62 },
    { LEVEL_AV1_6_3, MFX_LEVEL_AV1_63 },
    { LEVEL_AV1_7,  MFX_LEVEL_AV1_7 },
    { LEVEL_AV1_7_1, MFX_LEVEL_AV1_71 },
    { LEVEL_AV1_7_2, MFX_LEVEL_AV1_72 },
    { LEVEL_AV1_7_3,  MFX_LEVEL_AV1_73 }
};

static const std::pair<C2Config::profile_t, mfxU16> g_av1_profiles[] =
{
    { PROFILE_AV1_0, MFX_PROFILE_AV1_MAIN },
    { PROFILE_AV1_1, MFX_PROFILE_AV1_HIGH },
    { PROFILE_AV1_2, MFX_PROFILE_AV1_PRO }
};

bool Av1ProfileAndroidToMfx(C2Config::profile_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_av1_profiles, android_value, mfx_value);
}

bool Av1ProfileMfxToAndroid(mfxU16 mfx_value, C2Config::profile_t* android_value)
{
    return SecondToFirst(g_av1_profiles, mfx_value, android_value);
}

bool Av1LevelAndroidToMfx(C2Config::level_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_av1_levels, android_value, mfx_value);
}

bool Av1LevelMfxToAndroid(mfxU16 mfx_value, C2Config::level_t* android_value)
{
    return SecondToFirst(g_av1_levels, mfx_value, android_value);
}

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

bool Vp9ProfileAndroidToMfx(C2Config::profile_t android_value, mfxU16* mfx_value)
{
    return FirstToSecond(g_vp9_profiles, android_value, mfx_value);
}

bool Vp9ProfileMfxToAndroid(mfxU16 mfx_value, C2Config::profile_t* android_value)
{
    return SecondToFirst(g_vp9_profiles, mfx_value, android_value);
}

// Returns pointers to NV12 planes.
void InitNV12PlaneData(int32_t pitch_y, int32_t alloc_height, uint8_t* base, uint8_t** plane_data)
{
    plane_data[C2PlanarLayout::PLANE_Y] = base;
    plane_data[C2PlanarLayout::PLANE_U] = base + alloc_height * pitch_y;
    plane_data[C2PlanarLayout::PLANE_V] = base + alloc_height * pitch_y + 1;
}

void InitNV12PlaneLayout(uint32_t pitches[C2PlanarLayout::MAX_NUM_PLANES], C2PlanarLayout* layout)
{
    layout->type = C2PlanarLayout::TYPE_YUV;
    layout->numPlanes = 3;
    layout->rootPlanes = 2;

    C2PlaneInfo& y_plane = layout->planes[C2PlanarLayout::PLANE_Y];
    y_plane.channel = C2PlaneInfo::CHANNEL_Y;
    y_plane.colInc = 1;
    y_plane.rowInc = static_cast<int32_t>(pitches[C2PlanarLayout::PLANE_Y]);
    y_plane.colSampling = 1;
    y_plane.rowSampling = 1;
    y_plane.bitDepth = 8;
    y_plane.allocatedDepth = 8;
    y_plane.rightShift = 0;
    y_plane.endianness = C2PlaneInfo::NATIVE;
    y_plane.rootIx = C2PlanarLayout::PLANE_Y;
    y_plane.offset = 0;

    C2PlaneInfo& u_plane = layout->planes[C2PlanarLayout::PLANE_U];
    u_plane.channel = C2PlaneInfo::CHANNEL_CB;
    u_plane.offset = 0;

    C2PlaneInfo& v_plane = layout->planes[C2PlanarLayout::PLANE_V];
    v_plane.channel = C2PlaneInfo::CHANNEL_CR;
    v_plane.offset = 1;

    for (C2PlanarLayout::plane_index_t plane_index : { C2PlanarLayout::PLANE_U, C2PlanarLayout::PLANE_V }) {
        C2PlaneInfo& plane = layout->planes[plane_index];
        plane.colInc = 2;
        plane.rowInc = static_cast<int32_t>(pitches[C2PlanarLayout::PLANE_U]);
        plane.colSampling = 2;
        plane.rowSampling = 2;
        plane.bitDepth = 8;
        plane.allocatedDepth = 8;
        plane.rightShift = 0;
        plane.endianness = C2PlaneInfo::NATIVE;
        plane.rootIx = C2PlanarLayout::PLANE_U;
    }
}

int MfxFourCCToGralloc(mfxU32 fourcc, bool using_video_memory)
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_U32(fourcc);

    switch (fourcc)
    {
        case MFX_FOURCC_NV12:
            return using_video_memory ? HAL_PIXEL_FORMAT_NV12_Y_TILED_INTEL : HAL_PIXEL_FORMAT_NV12;
        case MFX_FOURCC_P010:
            return using_video_memory ? HAL_PIXEL_FORMAT_P010_INTEL : HAL_PIXEL_FORMAT_YCBCR_P010;
        default:
            return 0;
    }
}

bool operator==(const C2PlaneInfo& plane0, const C2PlaneInfo& plane1)
{
    bool res = false;
    do {
        if (plane0.channel != plane1.channel) break;
        if (plane0.colInc != plane1.colInc) break;
        if (plane0.rowInc != plane1.rowInc) break;
        if (plane0.colSampling != plane1.colSampling) break;
        if (plane0.rowSampling != plane1.rowSampling) break;
        if (plane0.allocatedDepth != plane1.allocatedDepth) break;
        if (plane0.bitDepth != plane1.bitDepth) break;
        if (plane0.rightShift != plane1.rightShift) break;
        if (plane0.endianness != plane1.endianness) break;
        if (plane0.rootIx != plane1.rootIx) break;
        if (plane0.offset != plane1.offset) break;
        res = true;
    } while (false);
    return res;
}

bool operator==(const C2PlanarLayout& layout0, const C2PlanarLayout& layout1)
{
    bool res = false;
    do {
        if (layout0.type != layout1.type) break;
        if (layout0.numPlanes != layout1.numPlanes) break;
        if (layout0.rootPlanes != layout1.rootPlanes) break;

        bool match = true;
        for (uint32_t i = 0; i < layout0.numPlanes; ++i) {
            if (!(layout0.planes[i] == layout1.planes[i])) {
                match = false;
                break;
            }
        }
        if (!match) break;

        res = true;
    } while (false);
    return res;
}

c2_status_t CopyGraphicView(const C2GraphicView* src, C2GraphicView* dst)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    do {
        if (src->width() != dst->width()) break;
        if (src->height() != dst->height()) break;

        C2PlanarLayout src_layout = src->layout();
        C2PlanarLayout dst_layout = dst->layout();

        if (!(src_layout == dst_layout)) {
            res = C2_CANNOT_DO; // copy if layouts match
            break;
        }

        uint32_t max_offsets[C2PlanarLayout::MAX_NUM_PLANES]{0};

        for (uint32_t i = 0; i < src_layout.numPlanes; ++i) {

            const C2PlaneInfo& plane = src_layout.planes[i];
            uint32_t plane_width = src->width() / plane.colSampling;
            uint32_t plane_height = src->height() / plane.rowSampling;
            uint32_t max_offset = plane.offset + plane.maxOffset(plane_width, plane_height);
            if (max_offset > max_offsets[plane.rootIx]) {
                max_offsets[plane.rootIx] = max_offset;
            }
        }

        for (uint32_t i = 0; i < src_layout.rootPlanes; ++i) {
            std::copy(src->data()[i], src->data()[i] + max_offsets[i], dst->data()[i]);
        }
        res = C2_OK;

    } while (false);

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

std::string FormatHex(const uint8_t* data, size_t len)
{
    std::ostringstream ss;
    ss << std::hex;
    for (size_t i = 0; i < len; ++i) {
        if (i > 40) {
            ss << std::dec << std::setw(0) << "... [" << len << "]";
            break;
        }
        ss << std::setw(2) << std::setfill('0') << (uint32_t)data[i] << " ";
    }
    return ss.str();
}

BinaryWriter::BinaryWriter(const std::string& dir,
    const std::vector<std::string>& sub_dirs, const std::string& name)
{
    MFX_DEBUG_TRACE_FUNC;

    std::stringstream full_name;
    full_name << dir << "/";

    std::lock_guard<std::mutex> lock(m_mutex);

    for(const std::string& sub_dir : sub_dirs) {
        full_name << sub_dir;

        int ret = mkdir(full_name.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        if (ret != 0 && errno != EEXIST) {
            MFX_DEBUG_TRACE_MSG("cannot create the path");
            return;
        }

        MFX_DEBUG_TRACE_STREAM(NAMED(full_name.str()));
        full_name << "/";
    }

    full_name << name;
    stream_.open(full_name.str().c_str(), std::fstream::trunc | std::fstream::binary);
}

YUVWriter::YUVWriter(const std::string& dir,
    const std::vector<std::string>& sub_dirs, const std::string& name)
{
    MFX_DEBUG_TRACE_FUNC;

    std::stringstream full_name;
    full_name << dir << "/";

    std::lock_guard<std::mutex> lock(m_mutex);

    for(const std::string& sub_dir : sub_dirs) {
        full_name << sub_dir;

        int ret = mkdir(full_name.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

        if (ret != 0 && errno != EEXIST) {
            MFX_DEBUG_TRACE_MSG("cannot create the path");
            return;
        }

        MFX_DEBUG_TRACE_STREAM(NAMED(full_name.str()));
        full_name << "/";
    }

    full_name << name;
    stream_.open(full_name.str().c_str()/*, std::ios::app*/);
}

void YUVWriter::Write(const uint8_t* yuv_data, int stride, int height, int frameIndex) {
    stream_ << "frame " << frameIndex << std::endl;

    for(int i = 0; i < height; i++) {
        for(int j = 0; j < stride; j++) {
            stream_ << (int)yuv_data[i*stride + j] << " ";
        }
        stream_ << std::endl;
    }
    stream_ << std::endl;

    auto ptr = &yuv_data[stride * height];
    for(int i = 0; i < height/2; i++) {
        for(int j = 0; j < stride; j++) {
            stream_ << (int)ptr[i*stride + j] << " ";
        }
        stream_ << std::endl;
    }
}

bool IsYUV420(const C2GraphicView &view) {
    const C2PlanarLayout &layout = view.layout();
    return (layout.numPlanes == 3
            && layout.type == C2PlanarLayout::TYPE_YUV
            && layout.planes[layout.PLANE_Y].channel == C2PlaneInfo::CHANNEL_Y
            && layout.planes[layout.PLANE_Y].allocatedDepth == 8
            && layout.planes[layout.PLANE_Y].bitDepth == 8
            && layout.planes[layout.PLANE_Y].rightShift == 0
            && layout.planes[layout.PLANE_Y].colSampling == 1
            && layout.planes[layout.PLANE_Y].rowSampling == 1
            && layout.planes[layout.PLANE_U].channel == C2PlaneInfo::CHANNEL_CB
            && layout.planes[layout.PLANE_U].allocatedDepth == 8
            && layout.planes[layout.PLANE_U].bitDepth == 8
            && layout.planes[layout.PLANE_U].rightShift == 0
            && layout.planes[layout.PLANE_U].colSampling == 2
            && layout.planes[layout.PLANE_U].rowSampling == 2
            && layout.planes[layout.PLANE_V].channel == C2PlaneInfo::CHANNEL_CR
            && layout.planes[layout.PLANE_V].allocatedDepth == 8
            && layout.planes[layout.PLANE_V].bitDepth == 8
            && layout.planes[layout.PLANE_V].rightShift == 0
            && layout.planes[layout.PLANE_V].colSampling == 2
            && layout.planes[layout.PLANE_V].rowSampling == 2);
}

bool IsNV12(const C2GraphicView &view) {
    if (!IsYUV420(view)) {
        return false;
    }
    const C2PlanarLayout &layout = view.layout();
    return (layout.rootPlanes == 2
            && layout.planes[layout.PLANE_U].colInc == 2
            && layout.planes[layout.PLANE_U].rootIx == layout.PLANE_U
            && layout.planes[layout.PLANE_U].offset == 0
            && layout.planes[layout.PLANE_V].colInc == 2
            && layout.planes[layout.PLANE_V].rootIx == layout.PLANE_U
            && layout.planes[layout.PLANE_V].offset == 1);
}

bool IsI420(const C2GraphicView &view) {
    if (!IsYUV420(view)) {
        return false;
    }
    const C2PlanarLayout &layout = view.layout();
    return (layout.rootPlanes == 3
            && layout.planes[layout.PLANE_U].colInc == 1
            && layout.planes[layout.PLANE_U].rootIx == layout.PLANE_U
            && layout.planes[layout.PLANE_U].offset == 0
            && layout.planes[layout.PLANE_V].colInc == 1
            && layout.planes[layout.PLANE_V].rootIx == layout.PLANE_V
            && layout.planes[layout.PLANE_V].offset == 0);
}

bool IsYV12(const C2GraphicView &view) {
    if (!IsYUV420(view)) {
        return false;
    }
    const C2PlanarLayout &layout = view.layout();
    return (layout.rootPlanes == 3
            && layout.planes[layout.PLANE_U].colInc == 1
            && layout.planes[layout.PLANE_U].rootIx == layout.PLANE_V
            && layout.planes[layout.PLANE_U].offset == 0
            && layout.planes[layout.PLANE_V].colInc == 1
            && layout.planes[layout.PLANE_V].rootIx == layout.PLANE_U
            && layout.planes[layout.PLANE_V].offset == 0);
}

bool IsP010(const C2GraphicView &view) {
    const C2PlanarLayout &layout = view.layout();
        return (layout.numPlanes == 3
            && layout.type == C2PlanarLayout::TYPE_YUV
            && layout.planes[layout.PLANE_Y].channel == C2PlaneInfo::CHANNEL_Y
            && layout.planes[layout.PLANE_Y].allocatedDepth == 16
            && layout.planes[layout.PLANE_Y].bitDepth == 10
            && layout.planes[layout.PLANE_Y].rightShift == 6
            && layout.planes[layout.PLANE_Y].colSampling == 1
            && layout.planes[layout.PLANE_Y].rowSampling == 1
            && layout.planes[layout.PLANE_U].channel == C2PlaneInfo::CHANNEL_CB
            && layout.planes[layout.PLANE_U].allocatedDepth == 16
            && layout.planes[layout.PLANE_U].bitDepth == 10
            && layout.planes[layout.PLANE_U].rightShift == 6
            && layout.planes[layout.PLANE_U].colSampling == 2
            && layout.planes[layout.PLANE_U].rowSampling == 2
            && layout.planes[layout.PLANE_V].channel == C2PlaneInfo::CHANNEL_CR
            && layout.planes[layout.PLANE_V].allocatedDepth == 16
            && layout.planes[layout.PLANE_V].bitDepth == 10
            && layout.planes[layout.PLANE_V].rightShift == 6
            && layout.planes[layout.PLANE_V].colSampling == 2
            && layout.planes[layout.PLANE_V].rowSampling == 2
            && layout.rootPlanes == 2
            && layout.planes[layout.PLANE_U].colInc == 4
            && layout.planes[layout.PLANE_U].rootIx == layout.PLANE_U
            && layout.planes[layout.PLANE_U].offset == 0
            && layout.planes[layout.PLANE_V].colInc == 4
            && layout.planes[layout.PLANE_V].rootIx == layout.PLANE_U
            && layout.planes[layout.PLANE_V].offset == 2);
}

void ParseGop(
        const std::shared_ptr<C2StreamGopTuning::output> gop,
        uint32_t &syncInterval, uint32_t &iInterval, uint32_t &maxBframes) {
    uint32_t syncInt = 1;
    uint32_t iInt = 1;
    for (size_t i = 0; i < gop->flexCount(); ++i) {
        const C2GopLayerStruct &layer = gop->m.values[i];
        if (layer.count == UINT32_MAX) {
            syncInt = 0;
        } else if (syncInt <= UINT32_MAX / (layer.count + 1)) {
            syncInt *= (layer.count + 1);
        }
        if ((layer.type_ & I_FRAME) == 0) {
            if (layer.count == UINT32_MAX) {
                iInt = 0;
            } else if (iInt <= UINT32_MAX / (layer.count + 1)) {
                iInt *= (layer.count + 1);
            }
        }
        if (layer.type_ == C2Config::picture_type_t(P_FRAME | B_FRAME)) {
            maxBframes = layer.count;
        }
    }
    if (syncInterval) {
        syncInterval = syncInt;
    }
    if (iInterval) {
        iInterval = iInt;
    }
}
