/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_defs.h"
#include "gtest_emulation.h"
#include "test_components.h"
#include "mfx_c2_utils.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "C2BlockAllocator.h"
#include "streams/h264/aud_mw_e.264.h"

#include <future>

using namespace android;

const uint64_t FRAME_DURATION_US = 33333; // 30 fps
const nsecs_t TIMEOUT_NS = MFX_SECOND_NS;

namespace {
    struct ComponentDesc
    {
        const char* component_name;
        int flags;
        status_t creation_status;
        std::vector<const StreamDescription> streams;
    };
}

static std::vector<const StreamDescription> h264_streams =
{
    { aud_mw_e_264 },
};

static ComponentDesc g_components_desc[] = {
    { "C2.h264vd", 0, C2_OK, h264_streams },
    { "C2.NonExistingDecoder", 0, C2_NOT_FOUND, {} },
};

static const ComponentDesc* GetComponentDesc(const std::string& component_name)
{
    const ComponentDesc* result = nullptr;
    for(const auto& desc : g_components_desc) {
        if(component_name == desc.component_name) {
            result = &desc;
            break;
        }
    }
    return result;
}

static std::map<std::string, std::shared_ptr<MfxC2Component>>& GetComponentsCache()
{
    static std::map<std::string, std::shared_ptr<MfxC2Component>> g_components;
    return g_components;
}

static std::shared_ptr<MfxC2Component> GetCachedComponent(const char* name)
{
    std::shared_ptr<MfxC2Component> result;
    auto& components_cache = GetComponentsCache(); // auto& is needed to have ref not a copy of cache

    auto it = components_cache.find(name);
    if(it != components_cache.end()) {
        result = it->second;
    }
    else {
        const ComponentDesc* desc = GetComponentDesc(name);
        ASSERT_NE(desc, nullptr);

        MfxC2Component* mfx_component;
        status_t status = MfxCreateC2Component(name, desc->flags, &mfx_component);
        EXPECT_EQ(status, desc->creation_status);
        if(desc->creation_status == C2_OK) {
            EXPECT_NE(mfx_component, nullptr);
            result = std::shared_ptr<MfxC2Component>(mfx_component);

            components_cache.emplace(name, result);
        }
    }
    return result;
}

// Assures that all decoding components might be successfully created.
// NonExistingDecoder cannot be created and C2_NOT_FOUND error is returned.
TEST(MfxDecoderComponent, Create)
{
    for(const auto& desc : g_components_desc) {

        std::shared_ptr<MfxC2Component> decoder = GetCachedComponent(desc.component_name);

        EXPECT_EQ(decoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;
    }
}

// Checks that all successfully created decoding components expose C2ComponentInterface
// and return correct information once queried (component name).
TEST(MfxDecoderComponent, intf)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        EXPECT_EQ(comp_intf->getName(), desc.component_name);
    } );
}

static void PrepareWork(uint32_t frame_index,
    std::unique_ptr<C2Work>* work,
    const StreamDescription& stream,
    StreamDescription::Region& region, bool header)
{
    *work = std::make_unique<C2Work>();
    C2BufferPack* buffer_pack = &((*work)->input);

    buffer_pack->flags = flags_t(0);
    if (header)
        buffer_pack->flags = flags_t(buffer_pack->flags | BUFFERFLAG_CODEC_CONFIG);
    if (region.offset + region.size == (size_t)(stream.data.end() - stream.data.begin()))
        buffer_pack->flags = flags_t(buffer_pack->flags | BUFFERFLAG_END_OF_STREAM);

    // Set up frame header properties:
    // timestamp is set to correspond to 30 fps stream.
    buffer_pack->ordinal.timestamp = FRAME_DURATION_US * frame_index;
    buffer_pack->ordinal.frame_index = frame_index;
    buffer_pack->ordinal.custom_ordinal = 0;

    do {

        std::shared_ptr<android::C2BlockAllocator> allocator;
        android::status_t sts = GetC2BlockAllocator(&allocator);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(allocator, nullptr);

        if(nullptr == allocator) break;

        C2MemoryUsage mem_usage = { C2MemoryUsage::kSoftwareRead, C2MemoryUsage::kSoftwareWrite };
        std::shared_ptr<C2LinearBlock> block;
        sts = allocator->allocateLinearBlock(region.size,
            mem_usage, &block);

        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(block, nullptr);

        if(nullptr == block) break;

        uint8_t* data = nullptr;
        sts = MapLinearBlock(*block, TIMEOUT_NS, &data);
        EXPECT_EQ(sts, C2_OK);
        EXPECT_NE(data, nullptr);

        memcpy(data, (char*)&stream.data.front() + region.offset, region.size);

        C2Event event;
        event.fire(); // pre-fire as buffer is already ready to use
        C2ConstLinearBlock const_block = block->share(0, region.size, event.fence());
        // make buffer of linear block
        C2BufferData buffer_data = const_block;
        std::shared_ptr<C2Buffer> buffer = std::make_shared<C2Buffer>(buffer_data);

        buffer_pack->buffers.push_back(buffer);

        std::unique_ptr<C2Worklet> worklet = std::make_unique<C2Worklet>();
        // C2 requires 1 allocator per required output item
        worklet->allocators.push_back(allocator);
        // C2 requires output items be allocated in buffers list and set to nulls
        worklet->output.buffers.push_back(nullptr);
        // work of 1 worklet
        (*work)->worklets.push_back(std::move(worklet));
    } while(false);
}

class DecoderConsumer : public C2ComponentListener
{
public:
    typedef std::function<void(const uint8_t* data, size_t length)> OnFrame;

public:
    DecoderConsumer(OnFrame on_frame)
        :on_frame_(on_frame)
        ,frame_submitted_(0)
        ,frame_decoded_(0)
    {
    }

    // future ready when validator got all expected frames
    std::future<void> GetFuture()
    {
        return done_.get_future();
    }

    void SetSubmittedFrames(uint64_t frame_submitted)
    {
        frame_submitted_ = frame_submitted;
    }

protected:
    void onWorkDone(
        std::weak_ptr<C2Component> component,
        std::vector<std::unique_ptr<C2Work>> workItems) override
    {
        (void)component;

        for(std::unique_ptr<C2Work>& work : workItems) {
            EXPECT_EQ(work->worklets_processed, 1);
            EXPECT_EQ(work->result, C2_OK);

            std::unique_ptr<C2Worklet>& worklet = work->worklets.front();
            C2BufferPack& buffer_pack = worklet->output;

            uint64_t frame_index = buffer_pack.ordinal.frame_index;

            EXPECT_EQ(buffer_pack.ordinal.timestamp, frame_index * FRAME_DURATION_US); // 30 fps

            EXPECT_EQ(!frame_submitted_ || (frame_index < frame_submitted_), true)
                << "unexpected frame_index value" << frame_index;

            ++frame_decoded_;

            std::unique_ptr<C2ConstGraphicBlock> graphic_block;
            C2Error sts = GetC2ConstGraphicBlock(buffer_pack, &graphic_block);
            EXPECT_EQ(sts, C2_OK);

            if(nullptr != graphic_block) {

                C2Rect crop = graphic_block->crop();
                EXPECT_NE(crop.mWidth, 0);
                EXPECT_NE(crop.mHeight, 0);


                std::unique_ptr<const C2GraphicView> c_graph_view;
                sts = MapConstGraphicBlock(*graphic_block, TIMEOUT_NS, &c_graph_view);

                const uint8_t* raw  = c_graph_view->data();

                EXPECT_EQ(sts, C2_OK);
                EXPECT_NE(raw, nullptr);

                std::shared_ptr<std::vector<uint8_t>> data_buffer = std::make_shared<std::vector<uint8_t>>();
                data_buffer->resize(crop.mWidth * crop.mHeight * 3 / 2);
                uint8_t* raw_cropped = &(data_buffer->front());
                uint8_t* raw_cropped_chroma = raw_cropped + crop.mWidth * crop.mHeight;
                const uint8_t* raw_chroma = raw + (crop.mLeft + crop.mWidth) * (crop.mTop + crop.mHeight);

                for (uint32_t i = 0; i < crop.mHeight; i++) {
                    memcpy(raw_cropped + i * crop.mWidth, raw + (i + crop.mTop) * (crop.mLeft + crop.mWidth) + crop.mLeft, crop.mWidth);
                }
                for (uint32_t i = 0; i < (crop.mHeight >> 1); i++) {
                    memcpy(raw_cropped_chroma + i * crop.mWidth, raw_chroma + (i + (crop.mTop >> 1)) * (crop.mLeft + crop.mWidth) + crop.mLeft, crop.mWidth);
                }

                if(nullptr != raw_cropped) {
                    on_frame_(raw_cropped, crop.mWidth * crop.mHeight * 3 / 2);
                }
            }
        }
        // if collected all expected frames
        if(frame_submitted_ && (frame_decoded_ == frame_submitted_)) {
            done_.set_value();
        }
    }

    void onTripped(std::weak_ptr<C2Component> component,
                           std::vector<std::shared_ptr<C2SettingResult>> settingResult) override
    {
        (void)component;
        (void)settingResult;
        EXPECT_EQ(true, false) << "onTripped callback shouldn't come";
    }

    void onError(std::weak_ptr<C2Component> component,
                         uint32_t errorCode) override
    {
        (void)component;
        (void)errorCode;
        EXPECT_EQ(true, false) << "onError callback shouldn't come";
    }

private:
    OnFrame on_frame_;
    uint64_t frame_submitted_; // number of the frames submitted for decoding
    uint64_t frame_decoded_;   // number of decoded frames
    std::promise<void> done_;  // fire when all expected frames came
};

static void Decode(
    std::shared_ptr<C2Component> component,
    std::shared_ptr<DecoderConsumer> validator,
    const StreamDescription& stream)
{
    component->registerListener(validator);

    status_t sts = component->start();
    EXPECT_EQ(sts, C2_OK);

    StreamReader reader(stream);
    uint32_t frame_index = 0;

    StreamDescription::Region region {};
    bool header = false;

    bool res = reader.Read(StreamReader::Slicing::Frame(), &region, &header);
    while(res) {
        // prepare worklet and push
        std::unique_ptr<C2Work> work;

        // insert input data
        PrepareWork(frame_index, &work, stream, region, header);
        std::list<std::unique_ptr<C2Work>> works;
        works.push_back(std::move(work));

        frame_index++;

        res = reader.Read(StreamReader::Slicing::Frame(), &region, &header);
        if (!res) {
            validator->SetSubmittedFrames(frame_index);
        }

        sts = component->queue_nb(&works);
        EXPECT_EQ(sts, C2_OK);
    }

    std::future<void> future = validator->GetFuture();
    std::future_status future_sts = future.wait_for(std::chrono::seconds(10));
    EXPECT_EQ(future_sts, std::future_status::ready) << " decoded less frames than expected";

    component->unregisterListener(validator);
    sts = component->stop();
    EXPECT_EQ(sts, C2_OK);
}

TEST(MfxDecoderComponent, DecodeBitExact)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc& desc, C2CompPtr comp, C2CompIntfPtr comp_intf) {

        const int TESTS_COUNT = 5;

        for(const StreamDescription& stream : desc.streams) {
            for(int i = 0; i < TESTS_COUNT; ++i) {

                CRC32Generator crc_generator;

                GTestBinaryWriter writer(std::ostringstream()
                    << comp_intf->getName() << "-" << i << ".nv12");

                DecoderConsumer::OnFrame on_frame = [&] (const uint8_t* data, size_t length) {
                    writer.Write(data, length);
                    crc_generator.AddData(data, length);
                };

                std::shared_ptr<DecoderConsumer> validator =
                    std::make_shared<DecoderConsumer>(on_frame);

                Decode(comp, validator, stream);

                EXPECT_EQ(crc_generator.GetCrc32(), stream.crc32) << "Pass " << i << " not equal to reference CRC32";
            }
        }
    } );
}

// Checks the correctness of all decoding components state machine.
// The component should be able to start from STOPPED (initial) state,
// stop from RUNNING state. Otherwise, C2_BAD_STATE should be returned.
TEST(MfxDecoderComponent, State)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr) {

        status_t sts = C2_OK;

        sts = comp->start();
        EXPECT_EQ(sts, C2_OK);

        sts = comp->start();
        EXPECT_EQ(sts, C2_BAD_STATE);

        sts = comp->stop();
        EXPECT_EQ(sts, C2_OK);

        sts = comp->stop();
        EXPECT_EQ(sts, C2_BAD_STATE);
    } );
}
