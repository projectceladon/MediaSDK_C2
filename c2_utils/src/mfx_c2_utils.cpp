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

using namespace android;

status_t MfxStatusToC2(mfxStatus mfx_status)
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
            return C2_UNSUPPORTED;

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

status_t GetC2ConstGraphicBlock(
    const C2BufferPack& buf_pack, std::unique_ptr<C2ConstGraphicBlock>* c_graph_block)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_BAD_VALUE;

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

status_t GetC2ConstLinearBlock(
    const C2BufferPack& buf_pack, std::unique_ptr<C2ConstLinearBlock>* c_lin_block)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_BAD_VALUE;

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

status_t MapConstGraphicBlock(
    const C2ConstGraphicBlock& c_graph_block, nsecs_t timeout, const uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        res = c_graph_block.fence().wait(timeout);
        if(C2_OK != res) break;

        C2Acquirable<const C2GraphicView> acquirable = c_graph_block.map();

        res = acquirable.wait(timeout);
        if(C2_OK != res) break;

        C2GraphicView graph_view = acquirable.get();
        *data = graph_view.data();

    } while(false);

    return res;
}

status_t MapGraphicBlock(
    C2GraphicBlock& graph_block, nsecs_t timeout, uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        C2Acquirable<C2GraphicView> acq_graph_view = graph_block.map();

        res = acq_graph_view.wait(timeout);
        if(C2_OK != res) break;

        C2GraphicView gr_view = acq_graph_view.get();
        *data = gr_view.data();

    } while(false);

    return res;
}

status_t MapConstLinearBlock(
    const C2ConstLinearBlock& c_lin_block, nsecs_t timeout, const uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        res = c_lin_block.fence().wait(timeout);
        if(C2_OK != res) break;

        C2Acquirable<C2ReadView> acq_read_view = c_lin_block.map();

        res = acq_read_view.wait(timeout);
        if(C2_OK != res) break;

        C2ReadView read_view = acq_read_view.get();
        *data = read_view.data();

    } while(false);

    return res;
}

status_t MapLinearBlock(
    C2LinearBlock& lin_block, nsecs_t timeout, uint8_t** data)
{
    MFX_DEBUG_TRACE_FUNC;

    C2Error res = C2_OK;

    do {
        if(nullptr == data) {
            res = C2_BAD_VALUE;
            break;
        }

        C2Acquirable<C2WriteView> acq_write_view = lin_block.map();

        res = acq_write_view.wait(timeout);
        if(C2_OK != res) break;

        C2WriteView write_view = acq_write_view.get();
        *data = write_view.data();

    } while(false);

    return res;
}

std::unique_ptr<C2SettingResult> MakeC2SettingResult(
    const C2ParamField& param_field,
    C2SettingResult::Failure failure,
    std::initializer_list<C2ParamField> conflicting_fields,
    const C2FieldSupportedValues* supported_values)
{
    std::unique_ptr<C2FieldSupportedValues> supported_values_unique;
    if(nullptr != supported_values) {
        supported_values_unique =
            std::make_unique<C2FieldSupportedValues>(*supported_values);
    }

    C2SettingResult* set_res = new C2SettingResult { param_field, failure,
        std::move(supported_values_unique), conflicting_fields };
    return std::unique_ptr<C2SettingResult>(set_res);
}

status_t GetAggregateStatus(std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

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

    MFX_DEBUG_TRACE_android_status_t(res);
    return res;
}

std::unique_ptr<C2SettingResult> FindC2Param(
    const std::vector<std::shared_ptr<C2ParamDescriptor>>& params_desc, const C2Param* param)
{
    MFX_DEBUG_TRACE_FUNC;

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
                typeA.baseIndex() == typeB.baseIndex();
        };

        if (std::any_of(params_desc.begin(), params_desc.end(), match_regardless_port)) {
            res = MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_PORT);
        } else {
            res = MakeC2SettingResult(C2ParamField(param), C2SettingResult::BAD_TYPE);
        }
    }

    return res;
}
