/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Work.h>

#include <functional>
#include <queue>
#include <thread>

#include "mfx_defs.h"
#include "mfx_debug.h"

// queue executing commands in dedicated thread
class MfxCmdQueue
{
public:
    MfxCmdQueue() = default;
    MFX_CLASS_NO_COPY(MfxCmdQueue);

public:
    void Start();

    void Stop();

    void Abort();

    template<class Task>
    void Push(Task&& task);

private:
    typedef std::function<void()> MfxCmd;

private:
    void WaitingPop(MfxCmd* command);

    void Shutdown(bool abort);

    void Process();

private:
    std::mutex mutex_; // push/pop protection
    std::queue<MfxCmd> data_;
    std::condition_variable condition_; // event from push to processor
    std::thread working_thread_;
    std::mutex shutdown_mutex_; // protect if shutdown is requested simultaneously from some threads
};

template<class Task>
void MfxCmdQueue::Push(Task&& task)
{
    MFX_DEBUG_TRACE(MFX_PTR_NAME(this));

    // incoming task could be a lambda move assignable, but not copy assignable
    // such lambdas aren't convertible to std::function stored in our queue
    // so we create copy assignable lambda - it captures shared_ptr to incoming task
    std::shared_ptr<Task> shared_task = std::make_shared<Task>(std::move(task));

    MfxCmd cmd = [shared_task] () { (*shared_task)(); }; // just call task

    std::lock_guard<std::mutex> lock(mutex_);
    data_.push(cmd);
    condition_.notify_one();
}
