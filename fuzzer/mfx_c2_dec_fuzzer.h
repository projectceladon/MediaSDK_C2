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

#pragma once

#include <stdint.h>
#include <chrono>
#include <condition_variable>
#include "mfx_c2_store.h"

#include <C2Component.h>
#include <C2Config.h>
#include <util/C2InterfaceUtils.h>
#include <C2BufferPriv.h>

namespace fuzzer {

#define C2FUZZER_ALIGN(_sz, _align) (((_sz) + ((_align)-1)) & ~((_align)-1))

constexpr auto kC2FuzzerTimeOut = std::chrono::milliseconds(5000);
constexpr int32_t kNumberOfC2WorkItems = 8;
constexpr uint32_t kWidthOfVideo = 3840;
constexpr uint32_t kHeightOfVideo = 2160;

using FrameData = std::tuple<uint8_t*, size_t, uint32_t>;

class Codec2Fuzzer
{
public:
    Codec2Fuzzer() = default;
    ~Codec2Fuzzer() { deInitDecoder(); }
    bool initDecoder();
    void deInitDecoder();
    void decodeFrames(const uint8_t* data, size_t size);

protected:
    void handleWorkDone(std::weak_ptr<C2Component> comp,
                        std::list<std::unique_ptr<C2Work>>& workItems);

private:
    bool m_eos = false;
    std::shared_ptr<MfxC2ComponentStore> m_store;
    std::shared_ptr<C2Component> m_component;
    std::shared_ptr<C2ComponentInterface> m_interface;
    std::condition_variable m_cv;
    std::condition_variable m_cvQueue;
    std::mutex m_mutexQueueLock;
    std::mutex m_mutexDecodeComplete;

    std::list<std::unique_ptr<C2Work>> m_workQueue;
    std::shared_ptr<C2BlockPool> m_linearPool;
    C2BlockPool::local_id_t m_blockPoolId;

private:
    // ported from framework
    class BufferSource {
    public:
        BufferSource(const uint8_t* data, size_t size) : m_data(data), m_size(size) {
            m_readIndex = (size <= kMarkerSize) ? 0 : (size - kMarkerSize);
        }
        ~BufferSource() {
            m_data = nullptr;
            m_size = 0;
            m_readIndex = 0;
            m_frameList.clear();
        }
        bool isEos() { return m_frameList.empty(); }
        void parse();
        FrameData getFrame();

    private:
        bool isMarker() {
            if ((kMarkerSize < m_size) && (m_readIndex < m_size - kMarkerSize)) {
                return (memcmp(&m_data[m_readIndex], kMarker, kMarkerSize) == 0);
            } else {
                return false;
            }
        }

        bool isCSDMarker(size_t position) {
            if ((kMarkerSuffixSize < m_size) && (position < m_size - kMarkerSuffixSize)) {
                return (memcmp(&m_data[position], kCsdMarkerSuffix, kMarkerSuffixSize) == 0);
            } else {
                return false;
            }
        }

        bool searchForMarker();

        const uint8_t* m_data = nullptr;
        size_t m_size = 0;
        size_t m_readIndex = 0;
        std::vector<FrameData> m_frameList;
        static constexpr uint8_t kMarker[] = "_MARK";
        static constexpr uint8_t kCsdMarkerSuffix[] = "_H_";
        static constexpr uint8_t kFrameMarkerSuffix[] = "_F_";
        // All markers should be 5 bytes long ( sizeof '_MARK' which is 5)
        static constexpr size_t kMarkerSize = (sizeof(kMarker) - 1);
        // All marker types should be 3 bytes long ('_H_', '_F_')
        static constexpr size_t kMarkerSuffixSize = 3;
    };

};

} // namespace fuzzer
