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

#pragma once

#include <C2Work.h>

#include <utils/Log.h>
#include <functional>
#include <queue>
#include <thread>
#include <atomic>

#include "mfx_defs.h"
#include "mfx_debug.h"

// queue executing commands in dedicated thread
class MfxCmdQueue
{
public:
    MfxCmdQueue() = default;
    ~MfxCmdQueue();
    MFX_CLASS_NO_COPY(MfxCmdQueue);

public:
    void Start();

    void Stop();

    void Pause();

    void Resume();

    void Abort();

    template<class Task>
    void Push(Task&& task);

    void WaitForEmpty();

private:
    typedef std::function<void()> MfxCmd;

private:
    void WaitingPop(MfxCmd* command);

    void Shutdown(bool abort);

    void Process();

private:
    std::mutex m_mutex; // push/pop protection
    std::queue<MfxCmd> m_data;
    bool m_bPaused{false};
    std::condition_variable m_condition; // event from push to processor
    std::condition_variable m_conditionEmpty; // event queue is clean
    std::thread m_workingThread;
    std::mutex m_threadMutex; // protect thread create/join operations
};

template<class Task>
void MfxCmdQueue::Push(Task&& task)
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));

    // incoming task could be a lambda move assignable, but not copy assignable
    // such lambdas aren't convertible to std::function stored in our queue
    // so we create copy assignable lambda - it captures shared_ptr to incoming task
    std::shared_ptr<Task> shared_task = std::make_shared<Task>(std::move(task));

    MfxCmd cmd = [moved_task = std::move(shared_task)] () { (*moved_task)(); }; // just call task

    std::lock_guard<std::mutex> lock(m_mutex);
    m_data.push(cmd);
    m_condition.notify_one();
}
