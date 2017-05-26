/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MFX_C2_COMPONENT_H__
#define __MFX_C2_COMPONENT_H__

#include <C2Component.h>

#include "mfx_defs.h"


class MfxC2Component : public android::C2ComponentInterface,
                       public android::C2Component,
                       public std::enable_shared_from_this<MfxC2Component>
{
protected:
    MfxC2Component(const android::C2String& name, int flags);
    MFX_CLASS_NO_COPY(MfxC2Component)

public:
    template<typename ComponentClass>
    static android::status_t Create(const char* name, int flags,
        MfxC2Component** component);
    virtual ~MfxC2Component();

private:
    virtual android::status_t Init() = 0;

protected: // android::C2ComponentInterface overrides
    android::C2String getName() const override;

    android::node_id getId() const override;

    status_t query_nb(
        const std::vector<android::C2Param* const> &stackParams,
        const std::vector<android::C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<android::C2Param>>* const heapParams) const override;

    status_t config_nb(
            const std::vector<android::C2Param* const> &params,
            std::vector<std::unique_ptr<android::C2SettingResult>>* const failures) override;

    status_t commit_sm(
            const std::vector<android::C2Param* const> &params,
            std::vector<std::unique_ptr<android::C2SettingResult>>* const failures) override;

    status_t createTunnel_sm(android::node_id targetComponent) override;

    status_t releaseTunnel_sm(android::node_id targetComponent) override;

    std::shared_ptr<android::C2ParamReflector> getParamReflector() const override;

    status_t getSupportedParams(
            std::vector<std::shared_ptr<android::C2ParamDescriptor>> * const params) const override;

    status_t getSupportedValues(
            const std::vector<const android::C2ParamField> fields,
            std::vector<android::C2FieldSupportedValues>* const values) const override;

protected: // android::C2Component
    status_t queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items) override;

    status_t announce_nb(const std::vector<android::C2WorkOutline> &items) override;

    status_t flush_sm(bool flushThrough, std::list<std::unique_ptr<android::C2Work>>* const flushedWork) override;

    status_t drain_nb(bool drainThrough) override;

    status_t start() override;

    status_t stop() override;

    void reset() override;

    void release() override;

    std::shared_ptr<C2ComponentInterface> intf() override;

protected: // variables
    android::C2String name_;

    int flags_ = 0;
};

template<typename ComponentClass>
android::status_t MfxC2Component::Create(const char* name, int flags,
    MfxC2Component** component)
{
    android::status_t result = android::C2_OK;
    MfxC2Component* component_created = new (std::nothrow) ComponentClass(name, flags);
    if(component_created != nullptr) {
        result = component_created->Init();
        if(result == android::C2_OK) {
            *component = component_created;
        }
        else {
            delete component_created;
        }
    }
    else {
        result = android::C2_NO_MEMORY;
    }
    return result;
}

#endif // #ifndef __MFX_C2_COMPONENT_H__
