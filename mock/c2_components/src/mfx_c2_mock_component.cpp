/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_mock_component.h"

#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"

using namespace android;

MfxC2MockComponent::MfxC2MockComponent(const android::C2String name, int flags, Type type) :
    MfxC2Component(name, flags), type_(type)
{
    MFX_DEBUG_TRACE_FUNC;
}

void MfxC2MockComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;
    registry.RegisterMfxC2Component("C2.MockComponent.Enc",
        &MfxC2Component::Factory<MfxC2MockComponent,Type>::Create<Encoder>);
    registry.RegisterMfxC2Component("C2.MockComponent.Dec",
        &MfxC2Component::Factory<MfxC2MockComponent, Type>::Create<Decoder>);
}

status_t MfxC2MockComponent::CopyGraphicToLinear(const C2BufferPack& input,
    const std::shared_ptr<C2BlockAllocator>& allocator, std::shared_ptr<C2Buffer>* out_buffer)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {
        std::unique_ptr<C2ConstGraphicBlock> const_graphic_block;

        res = GetC2ConstGraphicBlock(input, &const_graphic_block);
        if(C2_OK != res) break;

        uint32_t width = const_graphic_block->width();
        MFX_DEBUG_TRACE_U32(width);
        uint32_t height = const_graphic_block->height();
        MFX_DEBUG_TRACE_U32(height);

        const uint8_t* in_raw = nullptr;
        const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

        res = MapConstGraphicBlock(*const_graphic_block, TIMEOUT_NS, &in_raw);
        if(C2_OK != res) break;

        const size_t MEM_SIZE = width * height * 3 / 2;
        C2MemoryUsage mem_usage = { C2MemoryUsage::kSoftwareRead, C2MemoryUsage::kSoftwareWrite };
        std::shared_ptr<C2LinearBlock> out_block;

        res = allocator->allocateLinearBlock(MEM_SIZE, mem_usage, &out_block);
        if(C2_OK != res) break;

        uint8_t* out_raw = nullptr;

        res = MapLinearBlock(*out_block, TIMEOUT_NS, &out_raw);
        if(C2_OK != res) break;

        //  copy input buffer to output as is to identify data in test
        memcpy(out_raw, in_raw, MEM_SIZE);

        C2Event event;
        event.fire(); // pre-fire event as output buffer is ready to use
        C2ConstLinearBlock const_linear = out_block->share(0, out_block->capacity(), event.fence());
        C2BufferData out_buffer_data = const_linear;

        *out_buffer = std::make_shared<C2Buffer>(out_buffer_data);
    } while(false);

    return res;
}

static status_t GuessFrameSize(uint32_t buffer_size, uint32_t* width, uint32_t* height)
{
    uint32_t typical_frame_sizes[][2] = {
        { 320, 240 },
        { 640, 480 },
    };

    status_t res = C2_BAD_VALUE;
    for(const auto& frame_sizes : typical_frame_sizes) {
        if(frame_sizes[0] * frame_sizes[1] * 3 / 2 == buffer_size) {
            *width = frame_sizes[0];
            *height = frame_sizes[1];
            res = C2_OK;
            break;
        }
    }
    return res;
}

status_t MfxC2MockComponent::CopyLinearToGraphic(const C2BufferPack& input,
    const std::shared_ptr<C2BlockAllocator>& allocator, std::shared_ptr<C2Buffer>* out_buffer)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    do {
        std::unique_ptr<C2ConstLinearBlock> const_linear_block;

        res = GetC2ConstLinearBlock(input, &const_linear_block);
        if(C2_OK != res) break;

        uint32_t size = const_linear_block->size();
        MFX_DEBUG_TRACE_U32(size);

        uint32_t width = 0;
        uint32_t height = 0;
        res = GuessFrameSize(size, &width, &height);
        if(C2_OK != res) break;

        MFX_DEBUG_TRACE_U32(width);
        MFX_DEBUG_TRACE_U32(height);

        const uint8_t* in_raw = nullptr;
        const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

        res = MapConstLinearBlock(*const_linear_block, TIMEOUT_NS, &in_raw);
        if(C2_OK != res) break;

        const size_t MEM_SIZE = width * height * 3 / 2;
        C2MemoryUsage mem_usage = { C2MemoryUsage::kSoftwareRead, producer_memory_type_ };
        std::shared_ptr<C2GraphicBlock> out_block;

        res = allocator->allocateGraphicBlock(width, height, 0/*format*/, mem_usage, &out_block);
        if(C2_OK != res) break;
        {
            std::unique_ptr<C2GraphicView> out_view;
            res = MapGraphicBlock(*out_block, TIMEOUT_NS, &out_view);
            if(C2_OK != res) break;

            //  copy input buffer to output as is to identify data in test
            memcpy(out_view->data(), in_raw, MEM_SIZE);
        }
        C2Event event;
        event.fire(); // pre-fire event as output buffer is ready to use
        C2ConstGraphicBlock const_graphic = out_block->share(out_block->crop(), event.fence());
        C2BufferData out_buffer_data = const_graphic;

        *out_buffer = std::make_shared<C2Buffer>(out_buffer_data);
    } while(false);

    return res;
}

// Work processing method
// expects an input containing one graphic block in nv12 format
// with zero pitch, i.e block size is (width * height * 3 / 2) bytes.
// The method allocates linear block of the same size and copies all input bytes there.
// The purpose is to test buffers are going through C2 interfaces well.
void MfxC2MockComponent::DoWork(std::unique_ptr<android::C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    status_t res = C2_OK;

    do {
        if(work->worklets.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple worklets");
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
        const C2BufferPack& input = work->input;
        C2BufferPack& output = worklet->output;

        if(worklet->allocators.size() != 1 || worklet->output.buffers.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple outputs");
            res = C2_BAD_VALUE;
            break;
        }
        const std::shared_ptr<C2BlockAllocator>& allocator = worklet->allocators.front();
        //  form header of output data, copy input timestamps, etc. to identify data in test
        output.ordinal.timestamp = input.ordinal.timestamp;
        output.ordinal.frame_index = input.ordinal.frame_index;
        output.ordinal.custom_ordinal = input.ordinal.custom_ordinal;

        auto process_method = (type_ == Encoder) ?
            &MfxC2MockComponent::CopyGraphicToLinear : &MfxC2MockComponent::CopyLinearToGraphic;

        res = (this->*process_method)(input, allocator, &worklet->output.buffers.front());

    } while(false); // fake loop to have a cleanup point there

    NotifyWorkDone(std::move(work), res);
}

status_t MfxC2MockComponent::config_nb(
        const std::vector<android::C2Param* const> &params,
        std::vector<std::unique_ptr<android::C2SettingResult>>* const)
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;
    if (params.size() == 1 && params[0]->type() == C2ProducerMemoryType::typeIndex) {
        const C2ProducerMemoryType* param = (const C2ProducerMemoryType*)params[0];
        producer_memory_type_ = static_cast<C2MemoryUsage::Producer>(param->mValue);
        MFX_DEBUG_TRACE_I32(producer_memory_type_);
    } else {
        res = C2_BAD_VALUE;
    }
    return res;
}

status_t MfxC2MockComponent::queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& item : *items) {

        cmd_queue_.Push( [ work = std::move(item), this ] () mutable {
            DoWork(std::move(work));
        } );
    }

    return C2_OK;
}

android::status_t MfxC2MockComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    status_t res = C2_OK;

    if(type_ != Encoder && type_ != Decoder) {
        res = C2_CORRUPTED;
    }

    return res;
}

status_t MfxC2MockComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    cmd_queue_.Start();

    return C2_OK;
}

status_t MfxC2MockComponent::DoStop()
{
    MFX_DEBUG_TRACE_FUNC;

    cmd_queue_.Stop();

    return C2_OK;
}
