/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/
#define __C2_GENERATE_GLOBAL_VARS__
#include "mfx_c2_params.h"

#include "mfx_c2_component.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_components_registry.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_component"

MfxC2Component::MfxC2Component(const android::C2String& name, int flags) :
    name_(name),
    flags_(flags),
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
c2_status_t MfxC2Component::DoStop()
{
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

    return C2_OMITTED;
}

c2_status_t MfxC2Component::query_vb(
    const std::vector<C2Param*> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    android::c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    MFX_DEBUG_TRACE_FUNC;

    (void)stackParams;
    (void)heapParamIndices;
    (void)mayBlock;
    (void)heapParams;

    return C2_OMITTED;
}

c2_status_t MfxC2Component::config_vb(
    const std::vector<C2Param*> &params,
    c2_blocking_t mayBlock,
    std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)params;
    (void)mayBlock;
    (void)failures;

    return C2_OMITTED;
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

    return param_reflector_.getSupportedParams(params);
}

c2_status_t MfxC2Component::querySupportedValues_vb(
    std::vector<C2FieldSupportedValuesQuery> &fields, c2_blocking_t mayBlock) const
{
    MFX_DEBUG_TRACE_FUNC;

    (void)fields;
    (void)mayBlock;

    return C2_OMITTED;
}

c2_status_t MfxC2Component::queue_nb(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)items;

    return C2_OMITTED;
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
    (void)flushedWork;

    return C2_OMITTED;
}

c2_status_t MfxC2Component::drain_nb(drain_mode_t mode)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)mode;

    return C2_OMITTED;
}

c2_status_t MfxC2Component::start()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::lock_guard<std::mutex> lock(state_mutex_);

    if(State::STOPPED == state_) {

        res = DoStart();

        if(C2_OK == res) {
            state_ = State::RUNNING;
        }

    } else {
        res = C2_BAD_STATE;
    }

    return res;
}

c2_status_t MfxC2Component::stop()
{
    MFX_DEBUG_TRACE_FUNC;

    c2_status_t res = C2_OK;

    std::lock_guard<std::mutex> lock(state_mutex_);

    if(State::RUNNING == state_) {

        res = DoStop();

        if(C2_OK == res) {
            state_ = State::STOPPED;
        }

    } else {
        res = C2_BAD_STATE;
    }

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
    return C2_OMITTED;
}

std::shared_ptr<C2ComponentInterface> MfxC2Component::intf()
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<C2ComponentInterface> result = shared_from_this();

    MFX_DEBUG_TRACE_P(result.get());
    return result;
}

c2_status_t MfxC2Component::setListener_vb(
    const std::shared_ptr<Listener> &listener, android::c2_blocking_t mayBlock)
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

void MfxC2Component::NotifyWorkDone(std::unique_ptr<android::C2Work>&& work, android::c2_status_t sts)
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
        std::vector<std::unique_ptr<C2Work>> work_items;
        work_items.push_back(std::move(work));
        listener->onWorkDone_nb(weak_this, std::move(work_items));
    });
}
