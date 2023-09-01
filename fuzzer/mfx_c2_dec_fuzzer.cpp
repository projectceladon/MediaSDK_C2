// Copyright (c) 2017-2023 Intel Corporation
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

#include "mfx_c2_dec_fuzzer.h"
#include "mfx_debug.h"
#include "C2PlatformSupport.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_fuzzer"

namespace fuzzer {

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    MFX_DEBUG_TRACE_FUNC;

    if (size < 1) {
        return 0;
    }
    Codec2Fuzzer* codec = new Codec2Fuzzer();
    if (!codec) {
        return 0;
    }
    if (codec->initDecoder()) {
        codec->decodeFrames(data, size);
    }
    delete codec;
    return 0;
}

class LinearBuffer : public C2Buffer {
public:
    explicit LinearBuffer(const std::shared_ptr<C2LinearBlock>& block)
        : C2Buffer({block->share(block->offset(), block->size(), ::C2Fence())}) {}

    explicit LinearBuffer(const std::shared_ptr<C2LinearBlock>& block, size_t size)
        : C2Buffer({block->share(block->offset(), size, ::C2Fence())}) {}
};

struct CodecListener : public C2Component::Listener {
public:
    CodecListener(const std::function<void(std::weak_ptr<C2Component> comp,
                                            std::list<std::unique_ptr<C2Work>>& workItems)>
                        fn = nullptr)
        : callBack(fn) {}
    virtual void onWorkDone_nb(const std::weak_ptr<C2Component> comp,
                            std::list<std::unique_ptr<C2Work>> workItems) {
        if (callBack) {
            callBack(comp, workItems);
        }
    }

    virtual void onTripped_nb(const std::weak_ptr<C2Component> comp,
                            const std::vector<std::shared_ptr<C2SettingResult>> settingResults) {
        (void)comp;
        (void)settingResults;
    }

    virtual void onError_nb(const std::weak_ptr<C2Component> comp, uint32_t errorCode) {
        (void)comp;
        (void)errorCode;
    }

    std::function<void(std::weak_ptr<C2Component> comp,
                std::list<std::unique_ptr<C2Work>>& workItems)> callBack;
};

/**
 * Buffer source implementations to identify a frame and its size
 */
bool Codec2Fuzzer::BufferSource::searchForMarker() {
    MFX_DEBUG_TRACE_FUNC;

    while (true) {
        if (isMarker()) {
            return true;
        }
        --m_readIndex;
        if (m_readIndex > m_size) {
            break;
        }
    }
    return false;
}

void Codec2Fuzzer::BufferSource::parse() {
    MFX_DEBUG_TRACE_FUNC;

    bool isFrameAvailable = true;
    size_t bytesRemaining = m_size;
    while (isFrameAvailable) {
        isFrameAvailable = searchForMarker();
        if (isFrameAvailable) {
            size_t location = m_readIndex + kMarkerSize;
            bool isCSD = isCSDMarker(location);
            location += kMarkerSuffixSize;
            uint8_t* framePtr = const_cast<uint8_t*>(&m_data[location]);
            size_t frameSize = bytesRemaining - location;
            uint32_t flags = 0;
            if (m_frameList.empty()) {
                flags |= C2FrameData::FLAG_END_OF_STREAM;
            } else if (isCSD) {
                flags |= C2FrameData::FLAG_CODEC_CONFIG;
            }
            m_frameList.emplace_back(std::make_tuple(framePtr, frameSize, flags));
            bytesRemaining -= (frameSize + kMarkerSize + kMarkerSuffixSize);
            --m_readIndex;
        }
    }
    if (m_frameList.empty()) {
        /**
         * Scenario where input data does not contain the custom frame markers.
         * Hence feed the entire data as single frame.
         */
        m_frameList.emplace_back(
            std::make_tuple(const_cast<uint8_t*>(m_data), 0, C2FrameData::FLAG_END_OF_STREAM));
        m_frameList.emplace_back(
            std::make_tuple(const_cast<uint8_t*>(m_data), m_size, C2FrameData::FLAG_CODEC_CONFIG));
    }
}

FrameData Codec2Fuzzer::BufferSource::getFrame() {
    MFX_DEBUG_TRACE_FUNC;

    FrameData frame = m_frameList.back();
    m_frameList.pop_back();
    return frame;
}

bool Codec2Fuzzer::initDecoder() {
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t status = C2_OK;

    std::shared_ptr<C2AllocatorStore> allocatorStore = GetCodec2PlatformAllocatorStore();
    if (!allocatorStore) {
        return false;
    }

    std::shared_ptr<C2Allocator> linearAllocator;

    status = allocatorStore->fetchAllocator(C2AllocatorStore::DEFAULT_LINEAR, &linearAllocator);
    if (status != C2_OK) {
        return false;
    }

    m_linearPool = std::make_shared<C2PooledBlockPool>(linearAllocator, ++m_blockPoolId);
    if (!m_linearPool) {
        return false;
    }

    for (int32_t i = 0; i < kNumberOfC2WorkItems; ++i) {
        m_workQueue.emplace_back(new C2Work);
    }

    m_store.reset(MfxC2ComponentStore::Create(&status));
    if (!m_store) {
        return false;
    }

    status = m_store->createComponent(C2COMPONENTNAME, &m_component);
    if (status != C2_OK) {
        return false;
    }
    status = m_store->createInterface(C2COMPONENTNAME, &m_interface);
    if (status != C2_OK) {
        return false;
    }

    C2ComponentKindSetting kind;
    C2ComponentDomainSetting domain;
    status = m_interface->query_vb({&kind, &domain}, {}, C2_MAY_BLOCK, nullptr);
    if (status != C2_OK) {
        return false;
    }

    std::shared_ptr<C2Component::Listener> c2_listener(new CodecListener(
        [this](std::weak_ptr<C2Component> comp, std::list<std::unique_ptr<C2Work>>& workItems) {
            handleWorkDone(comp, workItems);
        }));

    if (!c2_listener) {
        return false;
    }

    status = m_component->setListener_vb(c2_listener, C2_DONT_BLOCK);
    if (status != C2_OK) {
        return false;
    }

    std::vector<C2Param*> configParams;
    C2StreamPictureSizeInfo::input inputSize(0u, kWidthOfVideo, kHeightOfVideo);
    if (C2Component::DOMAIN_VIDEO == domain.value && C2Component::KIND_DECODER == kind) {
        configParams.push_back(&inputSize);
    }
    else {
        MFX_DEBUG_TRACE_MSG("domain.value is NOT video or kind is NOT decoder");
        return false;
    }

    std::vector<std::unique_ptr<C2SettingResult>> failures;
    m_store->config_sm(configParams, &failures);

    if (failures.size() != 0) {
        return false;
    }

    status = m_component->start();
    if (status != C2_OK) {
        return false;
    }

    return true;
}

void Codec2Fuzzer::deInitDecoder() {
    m_component->stop();
    m_component->reset();
    m_component->release();
    m_component = nullptr;
}

void Codec2Fuzzer::decodeFrames(const uint8_t* data, size_t size) {
    MFX_DEBUG_TRACE_FUNC;
    std::unique_ptr<BufferSource> bufferSource = std::make_unique<BufferSource>(data, size);
    if (!bufferSource) {
        return;
    }
    bufferSource->parse();
    c2_status_t status = C2_OK;
    size_t numFrames = 0;
    while (!bufferSource->isEos()) {
        uint8_t* frame = nullptr;
        size_t frameSize = 0;
        FrameData frameData = bufferSource->getFrame();
        frame = std::get<0>(frameData);
        frameSize = std::get<1>(frameData);

        std::unique_ptr<C2Work> work;
        {
            std::unique_lock<std::mutex> lock(m_mutexQueueLock);
            if (m_workQueue.empty()) m_cvQueue.wait_for(lock, kC2FuzzerTimeOut);
            if (!m_workQueue.empty()) {
                work.swap(m_workQueue.front());
                m_workQueue.pop_front();
            } else {
                return;
            }
        }

        work->input.flags = (C2FrameData::flags_t)std::get<2>(frameData);
        work->input.ordinal.timestamp = 0;
        work->input.ordinal.frameIndex = ++numFrames;
        work->input.buffers.clear();
        int32_t alignedSize = C2FUZZER_ALIGN(frameSize, PAGE_SIZE);

        std::shared_ptr<C2LinearBlock> block;
        status = m_linearPool->fetchLinearBlock(
            alignedSize, {C2MemoryUsage::CPU_READ, C2MemoryUsage::CPU_WRITE}, &block);
        if (status != C2_OK || block == nullptr) {
            return;
        }

        C2WriteView view = block->map().get();
        if (view.error() != C2_OK) {
            return;
        }
        memcpy(view.base(), frame, frameSize);
        work->input.buffers.emplace_back(new LinearBuffer(block, frameSize));
        work->worklets.clear();
        work->worklets.emplace_back(new C2Worklet);

        std::list<std::unique_ptr<C2Work>> items;
        items.push_back(std::move(work));
        status = m_component->queue_nb(&items);
        if (status != C2_OK) {
            return;
        }
    }
    std::unique_lock<std::mutex> waitForDecodeComplete(m_mutexDecodeComplete);
    m_cv.wait_for(waitForDecodeComplete, kC2FuzzerTimeOut, [this] { return m_eos; });
    std::list<std::unique_ptr<C2Work>> c2flushedWorks;
    m_component->flush_sm(C2Component::FLUSH_COMPONENT, &c2flushedWorks);
}

void Codec2Fuzzer::handleWorkDone(std::weak_ptr<C2Component> comp,
                                  std::list<std::unique_ptr<C2Work>>& workItems) {
    MFX_DEBUG_TRACE_FUNC;

    (void)comp;
    for (std::unique_ptr<C2Work>& work : workItems) {
        if (!work->worklets.empty()) {
            if (work->worklets.front()->output.flags != C2FrameData::FLAG_INCOMPLETE) {
                m_eos = (work->worklets.front()->output.flags & C2FrameData::FLAG_END_OF_STREAM) != 0;
                work->input.buffers.clear();
                work->worklets.clear();
                {
                    std::unique_lock<std::mutex> lock(m_mutexQueueLock);
                    m_workQueue.push_back(std::move(work));
                    m_cvQueue.notify_all();
                }
                if (m_eos) {
                    std::lock_guard<std::mutex> waitForDecodeComplete(m_mutexDecodeComplete);
                    m_cv.notify_one();
                }
            }
        }
    }
}

} // namespace fuzzer
