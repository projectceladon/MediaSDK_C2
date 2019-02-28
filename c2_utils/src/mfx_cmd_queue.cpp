/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

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
