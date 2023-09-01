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

    // C2ComponentStore overrides
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
    std::map<std::string, ComponentDesc> m_componentsRegistry_;

    MfxXmlParser m_xmlParser;

    std::shared_ptr<C2ReflectorHelper> m_reflector = std::make_shared<C2ReflectorHelper>();
    mutable std::mutex m_reflectorMutex;
};
