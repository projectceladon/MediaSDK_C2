/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_c2_store.h

Defined functions:

Defined help functions:

*********************************************************************************/
#pragma once

#include <C2Component.h>
#include "mfx_c2_xml_parser.h"

#include <map>

#include "mfx_c2_component.h"
#include "mfx_c2_param_reflector.h"

using namespace android;

class MfxC2ComponentStore : public C2ComponentStore {
public:
    static MfxC2ComponentStore* Create(c2_status_t* status);

private: // C2ComponentStore overrides
    C2String getName() const override;

    c2_status_t createComponent(C2String name, std::shared_ptr<C2Component>* const component) override;

    c2_status_t createInterface(C2String name, std::shared_ptr<C2ComponentInterface>* const interface) override;

    std::vector<std::shared_ptr<const C2Component::Traits>> listComponents() override;

    c2_status_t copyBuffer(std::shared_ptr<C2GraphicBuffer> src, std::shared_ptr<C2GraphicBuffer> dst) override;

    c2_status_t query_sm(
        const std::vector<C2Param*> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<C2Param>>*const heapParams) const override;

    c2_status_t config_sm(
        const std::vector<C2Param*> &params,
        std::vector<std::unique_ptr<C2SettingResult>>*const failures) override;

    std::shared_ptr<C2ParamReflector> getParamReflector() const override;

    c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>> * const params) const override;

    c2_status_t querySupportedValues_sm(
            std::vector<C2FieldSupportedValuesQuery> &fields) const override;


private: // implementation methods
    c2_status_t readConfigFile();
    c2_status_t readXmlConfigFile();

    void* loadModule(const std::string& name);
private: // data
    struct ComponentDesc {
        std::string dso_name_;
        std::string media_type_;
        C2Component::kind_t kind_;
        MfxC2Component::CreateConfig config_;
        ComponentDesc(const char* dso_name,
            const char* media_type,
            C2Component::kind_t kind,
            const MfxC2Component::CreateConfig& config):
                dso_name_(dso_name),
                media_type_(media_type),
                kind_(kind),
                config_(config) {}
    };
    // this is a map between component names and component descriptions:
    //   (component's config, dso name, etc.)
    // no mutexed access needed as written only once before any read access
    std::map<std::string, ComponentDesc> components_registry_;

    MfxXmlParser xml_parser_;

    std::shared_ptr<MfxC2ParamReflector> reflector_ = std::make_shared<MfxC2ParamReflector>();
    mutable std::mutex reflector_mutex_;
};
