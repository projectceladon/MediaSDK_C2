/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Component.h>

#include "mfx_defs.h"
#include "mfx_c2_param_reflector.h"
#include <mutex>

class MfxC2Component : public C2ComponentInterface,
                       public C2Component,
                       public std::enable_shared_from_this<MfxC2Component>
{
protected:
    MfxC2Component(const C2String& name, int flags);
    MFX_CLASS_NO_COPY(MfxC2Component)

    // provides static Create method to registrate in components registry
    // variadic template args to be passed into component constructor
    template<typename ComponentClass, typename... ArgTypes>
    struct Factory;

public:
    virtual ~MfxC2Component();

private:
    virtual c2_status_t Init() = 0;

    virtual c2_status_t DoStart();

    virtual c2_status_t DoStop();

protected: // C2ComponentInterface overrides
    C2String getName() const override;

    c2_node_id_t getId() const override;

    c2_status_t query_vb(
        const std::vector<C2Param*> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        c2_blocking_t mayBlock,
        std::vector<std::unique_ptr<C2Param>>* const heapParams) const override;

    c2_status_t config_vb(
        const std::vector<C2Param*> &params,
        c2_blocking_t mayBlock,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures) override;

    c2_status_t createTunnel_sm(c2_node_id_t targetComponent) override;

    c2_status_t releaseTunnel_sm(c2_node_id_t targetComponent) override;

    c2_status_t querySupportedParams_nb(
        std::vector<std::shared_ptr<C2ParamDescriptor>>* const params) const override;

    c2_status_t querySupportedValues_vb(
        std::vector<C2FieldSupportedValuesQuery> &fields, c2_blocking_t mayBlock) const override;

protected: // C2Component
    c2_status_t queue_nb(std::list<std::unique_ptr<C2Work>>* const items) override;

    c2_status_t announce_nb(const std::vector<C2WorkOutline> &items) override;

    c2_status_t flush_sm(flush_mode_t mode, std::list<std::unique_ptr<C2Work>>* const flushedWork) override;

    c2_status_t drain_nb(drain_mode_t mode) override;

    c2_status_t start() override;

    c2_status_t stop() override;

    c2_status_t reset() override;

    c2_status_t release() override;

    std::shared_ptr<C2ComponentInterface> intf() override;

    c2_status_t setListener_vb(
        const std::shared_ptr<Listener> &listener, c2_blocking_t mayBlock) override;

protected:
    void NotifyListeners(std::function<void(std::shared_ptr<Listener>)> notify);

    void NotifyWorkDone(std::unique_ptr<C2Work>&& work, c2_status_t sts);

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

    C2String name_;

    int flags_ = 0;

    MfxC2ParamReflector param_reflector_;

    mfxIMPL mfx_implementation_;

private:
    std::list<std::shared_ptr<Listener>> listeners_;

    std::mutex listeners_mutex_;
};

template<typename ComponentClass, typename... ArgTypes>
struct MfxC2Component::Factory
{
    // method to create and init instance of component
    // variadic args are passed to constructor
    template<ArgTypes... arg_values>
    static MfxC2Component* Create(const char* name, int flags, c2_status_t* status)
    {
        c2_status_t result = C2_OK;
        // class to make constructor public and get access to new operator
        struct ConstructedClass : public ComponentClass
        {
        public:
            ConstructedClass(const char* name, int flags, ArgTypes... constructor_args) :
               ComponentClass(name, flags, constructor_args...) { }
        };

        MfxC2Component* component = new (std::nothrow) ConstructedClass(name, flags, arg_values...);
        if(component != nullptr) {
            result = component->Init();
            if(result != C2_OK) {
                delete component;
                component = nullptr;
            }
        }
        else {
            result = C2_NO_MEMORY;
        }

        if (nullptr != status) *status = result;
        return component;
    }
};
