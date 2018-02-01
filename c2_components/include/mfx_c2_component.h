/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Component.h>

#include "mfx_defs.h"
#include "mfx_c2_param_reflector.h"
#include <mutex>

class MfxC2Component : public android::C2ComponentInterface,
                       public android::C2Component,
                       public std::enable_shared_from_this<MfxC2Component>
{
protected:
    MfxC2Component(const android::C2String& name, int flags);
    MFX_CLASS_NO_COPY(MfxC2Component)

    // provides static Create method to registrate in components registry
    // variadic template args to be passed into component constructor
    template<typename ComponentClass, typename... ArgTypes>
    struct Factory;

public:
    virtual ~MfxC2Component();

private:
    virtual android::c2_status_t Init() = 0;

    virtual android::c2_status_t DoStart();

    virtual android::c2_status_t DoStop();

protected: // android::C2ComponentInterface overrides
    android::C2String getName() const override;

    android::node_id getId() const override;

    android::c2_status_t query_nb(
        const std::vector<android::C2Param* const> &stackParams,
        const std::vector<android::C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<android::C2Param>>* const heapParams) const override;

    android::c2_status_t config_nb(
            const std::vector<android::C2Param* const> &params,
            std::vector<std::unique_ptr<android::C2SettingResult>>* const failures) override;

    android::c2_status_t commit_sm(
            const std::vector<android::C2Param* const> &params,
            std::vector<std::unique_ptr<android::C2SettingResult>>* const failures) override;

    android::c2_status_t createTunnel_sm(android::node_id targetComponent) override;

    android::c2_status_t releaseTunnel_sm(android::node_id targetComponent) override;

    std::shared_ptr<android::C2ParamReflector> getParamReflector() const override;

    android::c2_status_t getSupportedParams(
            std::vector<std::shared_ptr<android::C2ParamDescriptor>>* const params) const override;

    android::c2_status_t getSupportedValues(
            const std::vector<const android::C2ParamField> fields,
            std::vector<android::C2FieldSupportedValues>* const values) const override;

protected: // android::C2Component
    android::c2_status_t queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items) override;

    android::c2_status_t announce_nb(const std::vector<android::C2WorkOutline> &items) override;

    android::c2_status_t flush_sm(bool flushThrough, std::list<std::unique_ptr<android::C2Work>>* const flushedWork) override;

    android::c2_status_t drain_nb(bool drainThrough) override;

    android::c2_status_t start() override;

    android::c2_status_t stop() override;

    void reset() override;

    void release() override;

    std::shared_ptr<C2ComponentInterface> intf() override;

    android::c2_status_t registerListener(std::shared_ptr<android::C2ComponentListener> listener) override;

    android::c2_status_t unregisterListener(std::shared_ptr<android::C2ComponentListener> listener) override;

protected:
    void NotifyListeners(std::function<void(std::shared_ptr<android::C2ComponentListener>)> notify);

    void NotifyWorkDone(std::unique_ptr<android::C2Work>&& work, android::c2_status_t sts);

protected:
    /* State diagram:

       +------- stop ------- ERROR
       |                       ^
       |                       |
       |                     error
       |                       |
       |  +-----start ----> RUNNING
       V  |                 | |  ^
    STOPPED <--- stop ------+ |  |
       ^                      |  |
       |                 config  |
       |                  error  |
       |                      |  start
       |                      V  |
       +------- stop ------- TRIPPED

    Operations permitted:
        Tunings could be applied in all states.
        Settings could be applied in STOPPED state only.
*/
    enum class State
    {
        STOPPED,
        RUNNING,
        TRIPPED,
        ERROR
    };

protected: // variables
    State state_ = State::STOPPED;

    std::mutex state_mutex_;

    android::C2String name_;

    int flags_ = 0;

    MfxC2ParamReflector param_reflector_;

    mfxIMPL mfx_implementation_;

private:
    std::list<std::shared_ptr<android::C2ComponentListener>> listeners_;

    std::mutex listeners_mutex_;
};

template<typename ComponentClass, typename... ArgTypes>
struct MfxC2Component::Factory
{
    // method to create and init instance of component
    // variadic args are passed to constructor
    template<ArgTypes... arg_values>
    static android::c2_status_t Create(const char* name, int flags, MfxC2Component** component)
    {
        android::c2_status_t result = android::C2_OK;
        // class to make constructor public and get access to new operator
        struct ConstructedClass : public ComponentClass
        {
        public:
            ConstructedClass(const char* name, int flags, ArgTypes... constructor_args) :
               ComponentClass(name, flags, constructor_args...) { }
        };

        MfxC2Component* component_created = new (std::nothrow) ConstructedClass(name, flags, arg_values...);
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
};
