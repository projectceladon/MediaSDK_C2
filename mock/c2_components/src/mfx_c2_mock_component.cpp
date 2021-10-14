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

#include "mfx_c2_mock_component.h"

#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_utils.h"

#include "C2PlatformSupport.h"

using namespace android;

MfxC2MockComponent::MfxC2MockComponent(const C2String name, const CreateConfig& config,
    std::shared_ptr<MfxC2ParamReflector> reflector, Type type) :
        MfxC2Component(name, config, std::move(reflector)), m_type(type)
{
    MFX_DEBUG_TRACE_FUNC;
}

void MfxC2MockComponent::RegisterClass(MfxC2ComponentsRegistry& registry)
{
    MFX_DEBUG_TRACE_FUNC;
    registry.RegisterMfxC2Component("c2.intel.mock.encoder",
        &MfxC2Component::Factory<MfxC2MockComponent,Type>::Create<Encoder>);
    registry.RegisterMfxC2Component("c2.intel.mock.decoder",
        &MfxC2Component::Factory<MfxC2MockComponent, Type>::Create<Decoder>);
}

c2_status_t MfxC2MockComponent::CopyGraphicToLinear(const C2FrameData& input,
    const std::shared_ptr<C2BlockPool>& allocator, std::shared_ptr<C2Buffer>* out_buffer)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    do {
        std::unique_ptr<C2ConstGraphicBlock> const_graphic_block;

        res = GetC2ConstGraphicBlock(input, &const_graphic_block);
        if(C2_OK != res) break;

        uint32_t width = const_graphic_block->width();
        MFX_DEBUG_TRACE_U32(width);
        uint32_t height = const_graphic_block->height();
        MFX_DEBUG_TRACE_U32(height);

        const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

        std::unique_ptr<const C2GraphicView> c_graph_view;
        res = MapConstGraphicBlock(*const_graphic_block, TIMEOUT_NS, &c_graph_view);
        if(C2_OK != res) break;
        const uint8_t* const* in_raw = c_graph_view->data();

        const size_t MEM_SIZE = width * height * 3 / 2;
        C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE };
        std::shared_ptr<C2LinearBlock> out_block;

        res = allocator->fetchLinearBlock(MEM_SIZE, mem_usage, &out_block);
        if(C2_OK != res) break;

        std::unique_ptr<C2WriteView> write_view;
        res = MapLinearBlock(*out_block, TIMEOUT_NS, &write_view);
        if(C2_OK != res) break;

        //  copy input buffer to output as is to identify data in test
        std::copy(in_raw[0], in_raw[0] + MEM_SIZE, write_view->data());

        //C2Event event;// not supported yet, left for future use
        //event.fire(); // pre-fire event as output buffer is ready to use
        C2ConstLinearBlock const_linear = out_block->share(0, out_block->capacity(), C2Fence()/*event.fence()*/);

        *out_buffer = std::make_shared<C2Buffer>(MakeC2Buffer( { const_linear } ));
    } while(false);

    return res;
}

static c2_status_t GuessFrameSize(uint32_t buffer_size, uint32_t* width, uint32_t* height)
{
    uint32_t typical_frame_sizes[][2] = {
        { 320, 240 },
        { 640, 480 },
    };

    c2_status_t res = C2_BAD_VALUE;
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

c2_status_t MfxC2MockComponent::CopyLinearToGraphic(const C2FrameData& input,
    const std::shared_ptr<C2BlockPool>& allocator, std::shared_ptr<C2Buffer>* out_buffer)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

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

        const c2_nsecs_t TIMEOUT_NS = MFX_SECOND_NS;
        std::unique_ptr<C2ReadView> read_view;
        res = MapConstLinearBlock(*const_linear_block, TIMEOUT_NS, &read_view);
        if(C2_OK != res) break;
        const uint8_t* in_raw = read_view->data();

        const size_t MEM_SIZE = width * height * 3 / 2;
        C2MemoryUsage mem_usage = { C2MemoryUsage::CPU_READ, m_uProducerMemoryType };
        std::shared_ptr<C2GraphicBlock> out_block;

        res = allocator->fetchGraphicBlock(width, height, HAL_PIXEL_FORMAT_NV12_TILED_INTEL, mem_usage, &out_block);
        if(C2_OK != res) break;
        {
            std::unique_ptr<C2GraphicView> out_view;
            res = MapGraphicBlock(*out_block, TIMEOUT_NS, &out_view);
            if(C2_OK != res) break;

            //  copy input buffer to output as is to identify data in test
            std::copy(in_raw, in_raw + MEM_SIZE, out_view->data()[0]);
        }
        // C2Event event; // not supported yet, left for future use
        // event.fire(); // pre-fire event as output buffer is ready to use
        C2ConstGraphicBlock const_graphic = out_block->share(out_block->crop(), C2Fence()/*event.fence()*/);
        C2Buffer out_gr_buffer = MakeC2Buffer( { const_graphic } );

        *out_buffer = std::make_shared<C2Buffer>(out_gr_buffer);
    } while(false);

    return res;
}

// Work processing method
// expects an input containing one graphic block in nv12 format
// with zero pitch, i.e block size is (width * height * 3 / 2) bytes.
// The method allocates linear block of the same size and copies all input bytes there.
// The purpose is to test buffers are going through C2 interfaces well.
void MfxC2MockComponent::DoWork(std::unique_ptr<C2Work>&& work)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_P(work.get());

    c2_status_t res = C2_OK;

    do {
        if (work == nullptr) {
            MFX_DEBUG_TRACE_MSG("work is nullptr");
            res = C2_BAD_VALUE;
            break;
        }

        if (work->worklets.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple worklets");
            res = C2_BAD_VALUE;
            break;
        }

        std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
        const C2FrameData& input = work->input;
        C2FrameData& output = worklet->output;

        if (worklet->output.buffers.size() != 1) {
            MFX_DEBUG_TRACE_MSG("Cannot handle multiple outputs");
            res = C2_BAD_VALUE;
            break;
        }

        if (!m_c2Allocator) {
            auto block_pool_id = (m_type == Encoder) ?
                C2BlockPool::BASIC_LINEAR : C2BlockPool::BASIC_GRAPHIC;

            res = GetCodec2BlockPool(block_pool_id,
                shared_from_this(), &m_c2Allocator);
            if (res != C2_OK) break;
        }

        // Pass end of stream flag only.
        output.flags = (C2FrameData::flags_t)(input.flags & C2FrameData::FLAG_END_OF_STREAM);
        //  form header of output data, copy input timestamps, etc. to identify data in test
        output.ordinal = input.ordinal;

        auto process_method = (m_type == Encoder) ?
            &MfxC2MockComponent::CopyGraphicToLinear : &MfxC2MockComponent::CopyLinearToGraphic;

        res = (this->*process_method)(input, m_c2Allocator, &worklet->output.buffers.front());

    } while(false); // fake loop to have a cleanup point there

    if (work) {
        NotifyWorkDone(std::move(work), res);
    } else {
        FatalError(res);
    }
}

c2_status_t MfxC2MockComponent::Config(
        std::unique_lock<std::mutex> state_lock,
        const std::vector<C2Param*> &params,
        c2_blocking_t mayBlock,
        std::vector<std::unique_ptr<C2SettingResult>>* const)
{
    (void)mayBlock;

    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    std::vector<std::shared_ptr<C2SettingResult>> tripped_reasons;

    for (C2Param* param : params) {

        switch (C2Param::Type(param->type()).type()) {
            case C2ProducerMemoryType::PARAM_TYPE: {
                const C2ProducerMemoryType* memory_param = (const C2ProducerMemoryType*)param;
                m_uProducerMemoryType = memory_param->value;
                MFX_DEBUG_TRACE_I32(m_uProducerMemoryType);
                break;
            }
            case C2TrippedTuning::PARAM_TYPE: {
                const C2TrippedTuning* tripped_tuning = (const C2TrippedTuning*)param;
                if (tripped_tuning->value) {
                    MFX_DEBUG_TRACE_MSG("Force component to tripped state.");
                    tripped_reasons.push_back(MakeC2SettingResult(
                        C2ParamField(tripped_tuning), C2SettingResult::BAD_VALUE));
                }
                break;
            }

            default:
                res = C2_BAD_VALUE;
                break;
        }
    }

    if (tripped_reasons.size() > 0) {
        state_lock.unlock(); // allow state to be changed
        ConfigError(tripped_reasons);
    }
    return res;
}

c2_status_t MfxC2MockComponent::queue_nb(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    for(auto& item : *items) {

        m_cmdQueue.Push( [ work = std::move(item), this ] () mutable {
            DoWork(std::move(work));
        } );
    }

    return C2_OK;
}

c2_status_t MfxC2MockComponent::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    if(m_type != Encoder && m_type != Decoder) {
        res = C2_CORRUPTED;
    }

    return res;
}

c2_status_t MfxC2MockComponent::DoStart()
{
    MFX_DEBUG_TRACE_FUNC;

    m_cmdQueue.Start();

    return C2_OK;
}

c2_status_t MfxC2MockComponent::DoStop(bool abort)
{
    MFX_DEBUG_TRACE_FUNC;

    if (abort) {
        m_cmdQueue.Abort();
    } else {
        m_cmdQueue.Stop();
    }

    return C2_OK;
}

c2_status_t MfxC2MockComponent::Pause()
{
    MFX_DEBUG_TRACE_FUNC;

    m_cmdQueue.Pause();

    return C2_OK;
}

c2_status_t MfxC2MockComponent::Resume()
{
    MFX_DEBUG_TRACE_FUNC;

    m_cmdQueue.Resume();

    return C2_OK;
}
