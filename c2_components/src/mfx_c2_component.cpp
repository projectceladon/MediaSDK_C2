/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_component.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_component"

MfxC2Component::MfxC2Component(const C2String& name, int flags, std::shared_ptr<MfxC2ParamReflector> reflector) :
    name_(name),
    flags_(flags),
    param_storage_(std::move(reflector)),
    mfx_implementation_(MFX_IMPLEMENTATION)
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2Component::~MfxC2Component()
{
    MFX_DEBUG_TRACE_FUNC;
}

c2_status_t MfxC2Component::DoStart()
{
    return C2_OK;
}

c2_status_t MfxC2Component::DoStop(bool abort)
{
    (void)abort;
    return C2_OK;
}

C2String MfxC2Component::getName() const
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_S(name_.c_str());

    return name_;
}

c2_node_id_t MfxC2Component::getId() const
{
    MFX_DEBUG_TRACE_FUNC;

    return {};
}

std::unique_lock<std::mutex> MfxC2Component::AcquireStableStateLock(bool may_block) const
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_lock<std::mutex> res;
    if (may_block) {
        std::unique_lock<std::mutex> lock(state_mutex_);
        cond_state_stable_.wait(lock, [this] () { return next_state_ == state_; } );
        res = std::move(lock);
    } else {
        // try lock state to check if current state is stable or transition
        std::unique_lock<std::mutex> lock(state_mutex_, std::try_to_lock);
        if (lock.owns_lock()) {
             if (next_state_ != state_) {
                // if locked state is transition - the method returned unlocked unique_lock as a failure
                lock.unlock();
            }
        }
        res = std::move(lock);
    }
    return res;
}

std::unique_lock<std::mutex> MfxC2Component::AcquireRunningStateLock(bool may_block) const
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_lock<std::mutex> res = AcquireStableStateLock(may_block);
    if (res) {
        const State RUNNING_STATES[]{State::RUNNING, State::TRIPPED, State::ERROR};
        bool running = std::find(std::begin(RUNNING_STATES), std::end(RUNNING_STATES), state_)
            != std::end(RUNNING_STATES);
        if (!running) {
            res.unlock();
        }
    }
    return res;
}

c2_status_t MfxC2Component::query_vb(
    const std::vector<C2Param*> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::unique_lock<std::mutex> lock = AcquireStableStateLock(mayBlock == C2_MAY_BLOCK);
    if (lock.owns_lock()) {
        if (State::RELEASED != state_) {
            res = Query(std::move(lock), stackParams, heapParamIndices, mayBlock, heapParams);
        } else {
            res = C2_BAD_STATE;
        }
    } else {
        res = C2_BLOCKING;
    }
    return res;
}

c2_status_t MfxC2Component::config_vb(
    const std::vector<C2Param*> &params,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::unique_lock<std::mutex> lock = AcquireStableStateLock(mayBlock == C2_MAY_BLOCK);
    if (lock.owns_lock()) {
        if (State::RELEASED != state_) {
            res = Config(std::move(lock), params, mayBlock, failures);
        } else {
            res = C2_BAD_STATE;
        }
    } else {
        res = C2_BLOCKING;
    }
    return res;
}

c2_status_t MfxC2Component::createTunnel_sm(c2_node_id_t targetComponent)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)targetComponent;

    return C2_OMITTED;
}

c2_status_t MfxC2Component::releaseTunnel_sm(c2_node_id_t targetComponent)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)targetComponent;

    return C2_OMITTED;
}

c2_status_t MfxC2Component::querySupportedParams_nb(
    std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const
{
    MFX_DEBUG_TRACE_FUNC;

    return param_storage_.getSupportedParams(params);
}

c2_status_t MfxC2Component::querySupportedValues_vb(
    std::vector<C2FieldSupportedValuesQuery> &queries, c2_blocking_t mayBlock) const
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::unique_lock<std::mutex> lock = AcquireStableStateLock(mayBlock == C2_MAY_BLOCK);
    if (lock.owns_lock()) {
        if (State::RELEASED != state_) {
            res = param_storage_.querySupportedValues_vb(queries, mayBlock);
        } else {
            res = C2_BAD_STATE;
        }
    } else {
        res = C2_BLOCKING;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2Component::queue_nb(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::unique_lock<std::mutex> lock = AcquireRunningStateLock(true/*may_block*/);
    if (lock) {
        res = Queue(items);
    } else {
        res = C2_BAD_STATE;
    }
    return res;
}

c2_status_t MfxC2Component::announce_nb(const std::vector<C2WorkOutline> &items)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)items;

    return C2_OMITTED;
}

c2_status_t MfxC2Component::flush_sm(flush_mode_t mode, std::list<std::unique_ptr<C2Work>>* const flushedWork)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)mode;

    c2_status_t res = C2_OK;

    std::unique_lock<std::mutex> lock = AcquireRunningStateLock(true/*may_block*/);
    if (lock) {
        res = Flush(flushedWork);
    } else {
        res = C2_BAD_STATE;
    }
    return res;
}

c2_status_t MfxC2Component::drain_nb(drain_mode_t mode)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)mode;

    return C2_OMITTED;
}

// Call it under state_mutex_ protection.
c2_status_t MfxC2Component::CheckStateTransitionConflict(const std::unique_lock<std::mutex>& state_lock,
    State next_state)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)state_lock;
    assert(state_lock.mutex() == &state_mutex_);
    assert(state_lock.owns_lock());

    c2_status_t res = C2_OK;
    if (next_state_ != state_) { // transition
        if (next_state_ == next_state) {
            // C2Component.h: when called during another same state change from another thread
            res = C2_DUPLICATE;
        } else {
            // C2Component.h: when called during another state change call from another
            // thread (user error)
            res = C2_BAD_STATE;
        }
    }
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2Component::start()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    // work to be done for state change
    std::function<c2_status_t()> action = [] () { return C2_CORRUPTED; };

    { // Pre-check of state change.
        std::unique_lock<std::mutex> lock(state_mutex_);
        res = CheckStateTransitionConflict(lock, State::RUNNING);
        if (C2_OK == res) {
            switch (state_) {
                case State::STOPPED:
                    next_state_ = State::RUNNING;
                    action = [this] () { return DoStart(); };
                    break;
                case State::TRIPPED:
                    next_state_ = State::RUNNING;
                    action = [this] () { return Resume(); };
                    break;
                default:
                    res = C2_BAD_STATE;
                    break;
            }
        }
    }

    if (C2_OK == res) {
        res = action();
        // Depending on res finish state change or roll it back
        {
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (C2_OK == res) {
                state_ = next_state_;
                 // next_state_ it maybe not RUNNING if Config/FatalError took place during action
            } else {
                next_state_ = state_;
            }
            cond_state_stable_.notify_all();
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2Component::stop()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    bool abort = false; // When stop from ERROR state all queued tasks should be abandoned.

    {
        std::unique_lock<std::mutex> lock(state_mutex_);
        res = CheckStateTransitionConflict(lock, State::STOPPED);
        if (C2_BAD_STATE == res) {
            if (State::ERROR == next_state_ || State::TRIPPED == next_state_) {
                // Transitions to ERROR and TRIPPED states are caused by the component itself,
                // should not give error, might wait for transition completion.
                cond_state_stable_.wait(lock, [this] () { return next_state_ == state_; } );
                res = C2_OK; // Suppress the error as transition completed.
            }
        }
        if (C2_OK == res) {
            switch (state_) {
                case State::RUNNING:
                case State::TRIPPED:
                    abort = false;
                    next_state_ = State::STOPPED;
                    break;
                case State::ERROR:
                    abort = true;
                    next_state_ = State::STOPPED;
                    break;
                default:
                    res = C2_BAD_STATE;
                    break;
            }
        }
    }

    if (C2_OK == res) {
        c2_status_t stop_res = DoStop(abort);
        MFX_DEBUG_TRACE__android_c2_status_t(stop_res);

        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = next_state_; // may not be STOPPED
        cond_state_stable_.notify_all();
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2Component::reset()
{
    MFX_DEBUG_TRACE_FUNC;
    return C2_OMITTED;
}

c2_status_t MfxC2Component::release()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    bool do_stop = false;

    { // Pre-check of state change.
        std::unique_lock<std::mutex> lock = AcquireStableStateLock(true/*may_block*/);
        if (lock.owns_lock()) {
            if (State::RELEASED == state_) {
                res = C2_DUPLICATE;
            } else {
                next_state_ = State::RELEASED;
                if (State::STOPPED != state_) {
                    do_stop = true;
                }
            }
        } else {
            res = C2_CORRUPTED;
        }
    }

    if (C2_OK == res) {

        c2_status_t stop_res = C2_OK;
        if (do_stop) {
            stop_res = DoStop(true/*abort*/);
            res = stop_res;
        }
        if (C2_OK == res) {
            res = Release();
        }

        {   // determine state we managed to reach
            std::lock_guard<std::mutex> lock(state_mutex_);
            if (C2_OK == res) { // required operations succeeded
                state_ = next_state_ = State::RELEASED;
            } else if (do_stop && (C2_OK == stop_res)) { // DoStop succeeded, Release failed
                state_ = next_state_ = State::STOPPED;
            } else { // no operations succeeded
                next_state_ = state_;
            }
            cond_state_stable_.notify_all();
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

std::shared_ptr<C2ComponentInterface> MfxC2Component::intf()
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<C2ComponentInterface> result = shared_from_this();

    MFX_DEBUG_TRACE_P(result.get());
    return result;
}

c2_status_t MfxC2Component::setListener_vb(
    const std::shared_ptr<Listener> &listener, c2_blocking_t mayBlock)
{
    (void)mayBlock;

    std::lock_guard<std::mutex> lock(listeners_mutex_);

    listeners_.clear(); // only one listener is allowed by documentation
    if (listener) {
        listeners_.push_back(listener);
    }
    return C2_OK;
}

void MfxC2Component::NotifyListeners(std::function<void(std::shared_ptr<Listener>)> notify)
{
    std::list<std::shared_ptr<Listener>> listeners_copy;
    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        listeners_copy = listeners_;
    }
    for(std::shared_ptr<Listener> listener : listeners_copy) {
        notify(listener);
    }
}

void MfxC2Component::NotifyWorkDone(std::unique_ptr<C2Work>&& work, c2_status_t sts)
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE__android_c2_status_t(sts);

    if(C2_OK == sts) {
        work->workletsProcessed = 1;
    }

    work->result = sts;

    std::weak_ptr<C2Component> weak_this = shared_from_this();

    NotifyListeners([weak_this, &work] (std::shared_ptr<Listener> listener)
    {
        std::list<std::unique_ptr<C2Work>> work_items;
        work_items.push_back(std::move(work));
        listener->onWorkDone_nb(weak_this, std::move(work_items));
    });
}

void MfxC2Component::ConfigError(const std::vector<std::shared_ptr<C2SettingResult>>& setting_result)
{
    MFX_DEBUG_TRACE_FUNC;

    bool do_pause = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        if (State::RUNNING == next_state_) {
            // has sense if in or going to less critical state
            next_state_ = State::TRIPPED;
            do_pause = true;
        }
    }

    if (do_pause) {
        c2_status_t pause_res = Pause();
        MFX_DEBUG_TRACE__android_c2_status_t(pause_res);

        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = next_state_;
        cond_state_stable_.notify_all();
    }

    std::weak_ptr<C2Component> weak_this = shared_from_this();
    NotifyListeners([weak_this, setting_result] (std::shared_ptr<Listener> listener)
    {
        listener->onTripped_nb(weak_this, setting_result);
    });
}

void MfxC2Component::FatalError(c2_status_t error)
{
    MFX_DEBUG_TRACE_FUNC;

    bool do_pause = false;
    {
        std::lock_guard<std::mutex> lock(state_mutex_);

        if (State::RUNNING == next_state_ || State::TRIPPED == next_state_) {
            // has sense if in or going to less critical state
            next_state_ = State::ERROR;
            do_pause = true;
        }
    }

    if (do_pause) {
        c2_status_t pause_res = Pause();
        MFX_DEBUG_TRACE__android_c2_status_t(pause_res);

        std::lock_guard<std::mutex> lock(state_mutex_);
        state_ = next_state_;
        cond_state_stable_.notify_all();
    }

    std::weak_ptr<C2Component> weak_this = shared_from_this();
    NotifyListeners([weak_this, error] (std::shared_ptr<Listener> listener)
    {
        listener->onError_nb(weak_this, error);
    });
}
