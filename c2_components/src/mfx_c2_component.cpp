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

#include "mfx_c2_component.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_components_monitor.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_component"

MfxC2Component::MfxC2Component(const C2String& name, const CreateConfig& config, std::shared_ptr<C2ReflectorHelper> reflector) :
    C2InterfaceHelper(std::move(reflector)),
    m_name(name),
    m_createConfig(config),
    m_mfxImplementation(MFX_IMPLEMENTATION)
{
    MFX_DEBUG_TRACE_FUNC;

    try
    {
        MfxC2ComponentsMonitor::getInstance().increase(m_name.c_str());
    }
    catch(const std::exception& e)
    {
        MFX_DEBUG_TRACE_STREAM("MfxC2ComponentsMonitor increase got exception: " << e.what() << '\n');
    }
}

MfxC2Component::~MfxC2Component()
{
    MFX_DEBUG_TRACE_FUNC;

    try
    {
        MfxC2ComponentsMonitor::getInstance().decrease(m_name.c_str());
    }
    catch(const std::exception& e)
    {
        MFX_DEBUG_TRACE_STREAM("MfxC2ComponentsMonitor decrease got exception: " << e.what() << '\n');
    }
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

    MFX_DEBUG_TRACE_S(m_name.c_str());

    return m_name;
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
        std::unique_lock<std::mutex> lock(m_stateMutex);
        m_condStateStable.wait(lock, [this] () { return m_nextState == m_state; } );
        res = std::move(lock);
    } else {
        // try lock state to check if current state is stable or transition
        std::unique_lock<std::mutex> lock(m_stateMutex, std::try_to_lock);
        if (lock.owns_lock()) {
             if (m_nextState != m_state) {
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
        bool running = std::find(std::begin(RUNNING_STATES), std::end(RUNNING_STATES), m_state)
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

    std::unique_lock<std::mutex> lock(m_releaseMutex, std::defer_lock);

    if (mayBlock == C2_MAY_BLOCK) {
        lock.lock();
    } else {
        lock.try_lock();
    }

    // Func query will return c2 params to framework, so we must update MFX params to c2 before calling it.
    if (lock.owns_lock()) {
        if (State::RELEASED != m_state) {
            res = UpdateMfxParamToC2(std::move(lock), stackParams, heapParamIndices, mayBlock, heapParams);
        } else {
            res = C2_BAD_STATE;
        }
    } else {
        res = C2_BLOCKING;
    }
    
    if (C2_OK != res)
    {
        MFX_DEBUG_TRACE__android_c2_status_t(res);
        return res;
    }
    
    res = query(stackParams, heapParamIndices, mayBlock, heapParams);
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    
    return C2_OK;
}

c2_status_t MfxC2Component::config_vb(
    const std::vector<C2Param*> &params,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;
    res = config(params, mayBlock, failures);

    std::unique_lock<std::mutex> lock = AcquireStableStateLock(mayBlock == C2_MAY_BLOCK);

    // Func config brings us the updated values which we have to sync to MFX params.
    if (lock.owns_lock()) {
        if (State::RELEASED != m_state) {
            res = UpdateC2ParamToMfx(std::move(lock), params, mayBlock, failures);
        } else {
            res = C2_BAD_STATE;
        }
    } else {
        res = C2_BLOCKING;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
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
    c2_status_t res = C2_OK;
    res = querySupportedParams(params);
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2Component::querySupportedValues_vb(
    std::vector<C2FieldSupportedValuesQuery> &queries, c2_blocking_t mayBlock) const
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;
    res = querySupportedValues(queries, mayBlock);
    for (C2FieldSupportedValuesQuery &query : queries) {
        if (C2_OK != query.status) {
            C2Param::Index ix = _C2ParamInspector::getIndex(query.field());
            MFX_DEBUG_TRACE_STREAM("param " << ix.typeIndex() << " query failed, status = "
                                << (int)query.status);
        }
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

    MFX_DEBUG_TRACE__android_c2_status_t(res);
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
    assert(state_lock.mutex() == &m_stateMutex);
    assert(state_lock.owns_lock());

    c2_status_t res = C2_OK;
    if (m_nextState != m_state) { // transition
        if (m_nextState == next_state) {
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

    // The creating component instances of the same type can't exceece the configured the number of maximum concurrent instances
    // And must ensure that C2_NO_MEMORY is returned by start() which because the reclaimResource calling happens in
    // MediaCodec::start() in libstagefright
    MFX_DEBUG_TRACE_I32(m_createConfig.concurrent_instances);
    if (MfxC2ComponentsMonitor::getInstance().get(m_name.c_str()) > m_createConfig.concurrent_instances) {
        MFX_DEBUG_TRACE_MSG("Cannot create component, the number of created components has exceeded maximum instance limit.");
        return C2_NO_MEMORY;
    }
    // work to be done for state change
    std::function<c2_status_t()> action = [] () { return C2_CORRUPTED; };

    { // Pre-check of state change.
        std::unique_lock<std::mutex> lock(m_stateMutex);
        res = CheckStateTransitionConflict(lock, State::RUNNING);
        if (C2_OK == res) {
            switch (m_state) {
                case State::STOPPED:
                    m_nextState = State::RUNNING;
                    action = [this] () { return DoStart(); };
                    break;
                case State::TRIPPED:
                    m_nextState = State::RUNNING;
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
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (C2_OK == res) {
                m_state = m_nextState;
                 // next_state_ it maybe not RUNNING if Config/FatalError took place during action
            } else {
                m_nextState = m_state;
            }
            m_condStateStable.notify_all();
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
        std::unique_lock<std::mutex> lock(m_stateMutex);
        res = CheckStateTransitionConflict(lock, State::STOPPED);
        if (C2_BAD_STATE == res) {
            if (State::ERROR == m_nextState || State::TRIPPED == m_nextState) {
                // Transitions to ERROR and TRIPPED states are caused by the component itself,
                // should not give error, might wait for transition completion.
                m_condStateStable.wait(lock, [this] () { return m_nextState == m_state; } );
                res = C2_OK; // Suppress the error as transition completed.
            }
        }
        if (C2_OK == res) {
            switch (m_state) {
                case State::RUNNING:
                case State::TRIPPED:
                    abort = false;
                    m_nextState = State::STOPPED;
                    break;
                case State::ERROR:
                    abort = true;
                    m_nextState = State::STOPPED;
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

        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_state = m_nextState; // may not be STOPPED
        m_condStateStable.notify_all();
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxC2Component::reset()
{
    MFX_DEBUG_TRACE_FUNC;
    if(m_state == State::STOPPED)
        return C2_OK;
    else
        return stop();
}

c2_status_t MfxC2Component::release()
{
    MFX_DEBUG_TRACE_FUNC;

    std::lock_guard<std::mutex> release_lock(m_releaseMutex);

    c2_status_t res = C2_OK;
    bool do_stop = false;

    { // Pre-check of state change.
        std::unique_lock<std::mutex> lock = AcquireStableStateLock(true/*may_block*/);
        if (lock.owns_lock()) {
            if (State::RELEASED == m_state) {
                res = C2_DUPLICATE;
            } else {
                m_nextState = State::RELEASED;
                if (State::STOPPED != m_state) {
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
            std::lock_guard<std::mutex> lock(m_stateMutex);
            if (C2_OK == res) { // required operations succeeded
                m_state = m_nextState = State::RELEASED;
            } else if (do_stop && (C2_OK == stop_res)) { // DoStop succeeded, Release failed
                m_state = m_nextState = State::STOPPED;
            } else { // no operations succeeded
                m_nextState = m_state;
            }
            m_condStateStable.notify_all();
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

    std::lock_guard<std::mutex> lock(m_listenersMutex);

    m_listeners.clear(); // only one listener is allowed by documentation
    if (listener) {
        m_listeners.push_back(listener);
    }
    return C2_OK;
}

void MfxC2Component::NotifyListeners(std::function<void(std::shared_ptr<Listener>)> notify)
{
    std::list<std::shared_ptr<Listener>> listeners_copy;
    {
        std::lock_guard<std::mutex> lock(m_listenersMutex);
        listeners_copy = m_listeners;
    }
    for(std::shared_ptr<Listener> listener : listeners_copy) {
        notify(std::move(listener));
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

    NotifyListeners([&weak_this, &work] (std::shared_ptr<Listener> listener)
    {
        std::list<std::unique_ptr<C2Work>> work_items;
        work_items.push_back(std::move(work));
        listener->onWorkDone_nb(std::move(weak_this), std::move(work_items));
    });
}

void MfxC2Component::ConfigError(const std::vector<std::shared_ptr<C2SettingResult>>& setting_result)
{
    MFX_DEBUG_TRACE_FUNC;

    bool do_pause = false;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        if (State::RUNNING == m_nextState) {
            // has sense if in or going to less critical state
            m_nextState = State::TRIPPED;
            do_pause = true;
        }
    }

    if (do_pause) {
        c2_status_t pause_res = Pause();
        MFX_DEBUG_TRACE__android_c2_status_t(pause_res);

        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_state = m_nextState;
        m_condStateStable.notify_all();
    }

    std::weak_ptr<C2Component> weak_this = shared_from_this();
    NotifyListeners([&weak_this, setting_result] (std::shared_ptr<Listener> listener)
    {
        listener->onTripped_nb(std::move(weak_this), setting_result);
    });
}

void MfxC2Component::FatalError(c2_status_t error)
{
    MFX_DEBUG_TRACE_FUNC;

    bool do_pause = false;
    {
        std::lock_guard<std::mutex> lock(m_stateMutex);

        if (State::RUNNING == m_nextState || State::TRIPPED == m_nextState) {
            // has sense if in or going to less critical state
            m_nextState = State::ERROR;
            do_pause = true;
        }
    }

    if (do_pause) {
        c2_status_t pause_res = Pause();
        MFX_DEBUG_TRACE__android_c2_status_t(pause_res);

        std::lock_guard<std::mutex> lock(m_stateMutex);
        m_state = m_nextState;
        m_condStateStable.notify_all();
    }

    std::weak_ptr<C2Component> weak_this = shared_from_this();
    NotifyListeners([&weak_this, error] (std::shared_ptr<Listener> listener)
    {
        listener->onError_nb(std::move(weak_this), error);
    });
}
