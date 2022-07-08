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


#include "mfx_cmd_queue.h"
#include "mfx_debug.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_cmd_queue"

MfxCmdQueue::~MfxCmdQueue()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    Abort();
}

void MfxCmdQueue::Start()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));

    std::lock_guard<std::mutex> lock(m_threadMutex);
    if(!m_workingThread.joinable()) {
        m_workingThread = std::thread(std::bind(&MfxCmdQueue::Process, this));
    }
}

void MfxCmdQueue::Stop()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    bool abort = false;
    Shutdown(abort);
}

void MfxCmdQueue::Pause()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bPaused = true;
}

void MfxCmdQueue::Resume()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    std::lock_guard<std::mutex> lock(m_mutex);
    m_bPaused = false;
    m_condition.notify_one();
}

void MfxCmdQueue::Abort()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    bool abort = true;
    Shutdown(abort);
}

void MfxCmdQueue::WaitingPop(MfxCmd* command)
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_data.empty()) {
        m_conditionEmpty.notify_one();
    }
    m_condition.wait(lock, [this] { return !m_bPaused && !m_data.empty(); });
    *command = m_data.front();
    m_data.pop();
}

void MfxCmdQueue::WaitForEmpty()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    std::unique_lock<std::mutex> lock(m_mutex);
    m_conditionEmpty.wait(lock, [this] { return m_data.empty(); });
}

void MfxCmdQueue::Shutdown(bool abort)
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if(abort) {
            m_data = std::queue<MfxCmd>();
        }
        m_data.push(MfxCmd()); // nullptr command is a stop thread command
        m_bPaused = false;
        m_condition.notify_one();
    }
    {
        // mutexed code section to not have exception in join
        // if already joined in another thread or not started
        std::lock_guard<std::mutex> lock(m_threadMutex);
        if(m_workingThread.joinable()) {
            m_workingThread.join();
        }
    }
}

void MfxCmdQueue::Process()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    for(;;) {
        MfxCmd mfx_cmd;
        WaitingPop(&mfx_cmd);
        if(mfx_cmd == nullptr) {
            break;
        } else {
            mfx_cmd();
        }
    }
}
