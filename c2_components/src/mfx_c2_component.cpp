/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_component.h"
#include "mfx_debug.h"
#include "mfx_c2_components_registry.h"

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_component"

MfxC2Component::MfxC2Component(const android::C2String& name, int flags) :
    name_(name), flags_(flags)
{
    MFX_DEBUG_TRACE_FUNC;
}

MfxC2Component::~MfxC2Component()
{
    MFX_DEBUG_TRACE_FUNC;
}

C2String MfxC2Component::getName() const
{
    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_S(name_.c_str());

    return name_;
}

node_id MfxC2Component::getId() const
{
    MFX_DEBUG_TRACE_FUNC;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::query_nb(
    const std::vector<C2Param* const> &stackParams,
    const std::vector<C2Param::Index> &heapParamIndices,
    std::vector<std::unique_ptr<C2Param>>* const heapParams) const
{
    MFX_DEBUG_TRACE_FUNC;

    (void)stackParams;
    (void)heapParamIndices;
    (void)heapParams;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::config_nb(
        const std::vector<C2Param* const> &params,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)params;
    (void)failures;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::commit_sm(
        const std::vector<C2Param* const> &params,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)params;
    (void)failures;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::createTunnel_sm(node_id targetComponent)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)targetComponent;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::releaseTunnel_sm(node_id targetComponent)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)targetComponent;

    return C2_NOT_IMPLEMENTED;
}

std::shared_ptr<C2ParamReflector> MfxC2Component::getParamReflector() const
{
    MFX_DEBUG_TRACE_FUNC;

    return nullptr;
}

status_t MfxC2Component::getSupportedParams(
        std::vector<std::shared_ptr<C2ParamDescriptor>> * const params) const
{
    MFX_DEBUG_TRACE_FUNC;

    (void)params;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::getSupportedValues(
        const std::vector<const C2ParamField> fields,
        std::vector<C2FieldSupportedValues>* const values) const
{
    MFX_DEBUG_TRACE_FUNC;

    (void)fields;
    (void)values;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::queue_nb(std::list<std::unique_ptr<C2Work>>* const items)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)items;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::announce_nb(const std::vector<C2WorkOutline> &items)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)items;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::flush_sm(bool flushThrough, std::list<std::unique_ptr<C2Work>>* const flushedWork)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)flushThrough;
    (void)flushedWork;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::drain_nb(bool drainThrough)
{
    MFX_DEBUG_TRACE_FUNC;

    (void)drainThrough;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::start()
{
    MFX_DEBUG_TRACE_FUNC;

    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2Component::stop()
{
    MFX_DEBUG_TRACE_FUNC;

    return C2_NOT_IMPLEMENTED;
}

void MfxC2Component::reset()
{
    MFX_DEBUG_TRACE_FUNC;
}

void MfxC2Component::release()
{
    MFX_DEBUG_TRACE_FUNC;
}

std::shared_ptr<C2ComponentInterface> MfxC2Component::intf()
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<C2ComponentInterface> result = shared_from_this();

    MFX_DEBUG_TRACE_P(result.get());
    return result;
}
