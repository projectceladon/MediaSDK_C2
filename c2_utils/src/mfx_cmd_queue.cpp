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

    std::lock_guard<std::mutex> lock(thread_mutex_);
    if(!working_thread_.joinable()) {
        working_thread_ = std::thread(std::bind(&MfxCmdQueue::Process, this));
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
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = true;
}

void MfxCmdQueue::Resume()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    std::lock_guard<std::mutex> lock(mutex_);
    paused_ = false;
    condition_.notify_one();
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
    std::unique_lock<std::mutex> lock(mutex_);
    if (data_.empty()) {
        condition_empty_.notify_one();
    }
    condition_.wait(lock, [this] { return !paused_ && !data_.empty(); });
    *command = data_.front();
    data_.pop();
}

void MfxCmdQueue::WaitForEmpty()
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    std::unique_lock<std::mutex> lock(mutex_);
    condition_empty_.wait(lock, [this] { return data_.empty(); });
}

void MfxCmdQueue::Shutdown(bool abort)
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if(abort) {
            data_ = std::queue<MfxCmd>();
        }
        data_.push(MfxCmd()); // nullptr command is a stop thread command
        paused_ = false;
        condition_.notify_one();
    }
    {
        // mutexed code section to not have exception in join
        // if already joined in another thread or not started
        std::lock_guard<std::mutex> lock(thread_mutex_);
        if(working_thread_.joinable()) {
            working_thread_.join();
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
        }
        else {
            mfx_cmd();
        }
    }
}
