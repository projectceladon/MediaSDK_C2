/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_c2_store.cpp

Defined functions:

Defined help functions:

*********************************************************************************/
#include "mfx_c2_store.h"
#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_component.h"

#include <dlfcn.h>
#include <fstream>

static const std::string FIELD_SEP = " : ";

using namespace android;

MfxC2ComponentStore* MfxC2ComponentStore::Create(c2_status_t* status) {

    MFX_DEBUG_TRACE_FUNC;

    MfxC2ComponentStore* store = new (std::nothrow)MfxC2ComponentStore();
    if (store != nullptr) {
        c2_status_t read_xml_cfg_res = store->readXmlConfigFile();
        c2_status_t read_cfg_res = store->readConfigFile();
        if (read_cfg_res != C2_OK || read_xml_cfg_res != C2_OK) {
            *status = (read_cfg_res != C2_OK) ? read_cfg_res : read_xml_cfg_res;
            delete store;
            store = nullptr;
        }
    } else {
        *status = C2_NO_MEMORY;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(*status);
    MFX_DEBUG_TRACE_P(store);
    return store;
}

C2String MfxC2ComponentStore::getName() const
{
    MFX_DEBUG_TRACE_FUNC;

    return MFX_C2_COMPONENT_STORE_NAME;
}


c2_status_t MfxC2ComponentStore::createComponent(C2String name, std::shared_ptr<C2Component>* const component) {

    MFX_DEBUG_TRACE_FUNC;

    c2_status_t result = C2_OK;
    if(component != nullptr) {

        auto it = components_registry_.find(name);
        if(it != components_registry_.end()) {

            auto dso_deleter = [] (void* handle) { dlclose(handle); };
            std::unique_ptr<void, decltype(dso_deleter)> dso(loadModule(it->second.dso_name_), dso_deleter);
            if(dso != nullptr) {

                CreateMfxC2ComponentFunc* create_func =
                    reinterpret_cast<CreateMfxC2ComponentFunc*>(dlsym(dso.get(), CREATE_MFX_C2_COMPONENT_FUNC_NAME));
                if(create_func != nullptr) {

                    std::shared_ptr<MfxC2ParamReflector> reflector;
                    {
                        std::lock_guard<std::mutex> lock(reflector_mutex_);
                        reflector = reflector_; // safe copy
                    }

                    MfxC2Component* mfx_component = (*create_func)(name.c_str(), it->second.config_, std::move(reflector), &result);
                    if(result == C2_OK) {
                        void* dso_handle = dso.release(); // release handle to be captured into lambda deleter
                        auto component_deleter = [dso_handle] (MfxC2Component* p) { delete p; dlclose(dso_handle); };
                        *component = std::shared_ptr<MfxC2Component>(mfx_component, component_deleter);
                    }
                }
                else {
                    MFX_LOG_ERROR("Module %s is invalid", it->second.dso_name_.c_str());
                    result = C2_NOT_FOUND;
                }
            }
            else {
                MFX_LOG_ERROR("Cannot load module %s", it->second.dso_name_.c_str());
                result = C2_NOT_FOUND;
            }
        }
        else {
            MFX_LOG_ERROR("Cannot find component %s", name.c_str());
            result = C2_NOT_FOUND;
        }
    }
    else {
        MFX_LOG_ERROR("output component ptr is null");
        result = C2_BAD_VALUE;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(result);
    return result;
}

c2_status_t MfxC2ComponentStore::createInterface(C2String name, std::shared_ptr<C2ComponentInterface>* const interface) {

    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<C2Component> component;
    c2_status_t result = createComponent(name, &component);

    if(result == C2_OK) {
        *interface = component->intf();
    }
    return result;
}

std::vector<std::shared_ptr<const C2Component::Traits>> MfxC2ComponentStore::listComponents() {

    MFX_DEBUG_TRACE_FUNC;
    std::vector<std::shared_ptr<const C2Component::Traits>> result;

    try {
        for(const auto& it_pair : components_registry_ ) {
            std::unique_ptr<C2Component::Traits> info = std::make_unique<C2Component::Traits>();
            info->name = it_pair.first;
            info->mediaType = it_pair.second.media_type_;
            MFX_DEBUG_TRACE_S(info->name.c_str());
            MFX_DEBUG_TRACE_S(info->mediaType.c_str());
            result.push_back(std::move(info));
        }
    }
    catch(std::exception& e) {
        MFX_DEBUG_TRACE_MSG("Exception caught");
        MFX_DEBUG_TRACE_S(e.what());
    }

    return result;
}

c2_status_t MfxC2ComponentStore::copyBuffer(std::shared_ptr<C2GraphicBuffer> src, std::shared_ptr<C2GraphicBuffer> dst) {

    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_MSG("Unimplemented method is called");

    (void)src;
    (void)dst;
    return C2_OMITTED;
}

c2_status_t MfxC2ComponentStore::query_sm(
        const std::vector<C2Param*> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<C2Param>>*const heapParams) const {

    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_MSG("Unimplemented method is called");

    (void)stackParams;
    (void)heapParamIndices;
    (void)heapParams;
    return C2_OMITTED;
}

c2_status_t MfxC2ComponentStore::config_sm(
        const std::vector<C2Param*> &params,
        std::vector<std::unique_ptr<C2SettingResult>>*const failures) {

    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_MSG("Unimplemented method is called");

    (void)params;
    (void)failures;
    return C2_OMITTED;
}

std::shared_ptr<C2ParamReflector> MfxC2ComponentStore::getParamReflector() const
{
    MFX_DEBUG_TRACE_FUNC;
    std::shared_ptr<MfxC2ParamReflector> reflector;
    {
        std::lock_guard<std::mutex> lock(reflector_mutex_);
        reflector = reflector_; // safe copy
    }
    return reflector;
}

c2_status_t MfxC2ComponentStore::querySupportedParams_nb(
        std::vector<std::shared_ptr<C2ParamDescriptor>> * const params) const
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_MSG("Unimplemented method is called");

    (void)params;
    return C2_OMITTED;
}

c2_status_t MfxC2ComponentStore::querySupportedValues_sm(
        std::vector<C2FieldSupportedValuesQuery> &fields) const
{
    MFX_DEBUG_TRACE_FUNC;
    MFX_DEBUG_TRACE_MSG("Unimplemented method is called");

    (void)fields;
    return C2_OMITTED;
}


/* Searches in the given line field which is separated from other lines by
 * FILED_SEP characters, Returns std::string which includes the field and
 * its size.
 */
static void MfxC2GetField(const std::string &line, std::string *str, size_t *str_pos)
{
    MFX_DEBUG_TRACE_FUNC;

    if (str == nullptr || str_pos == nullptr) return;

    *str = "";

    if (line.empty() || *str_pos == std::string::npos) return;

    MFX_DEBUG_TRACE_S(line.c_str());

    size_t pos = line.find_first_of(FIELD_SEP, *str_pos);

    size_t copy_size = (pos != std::string::npos) ? pos - *str_pos : line.size() - *str_pos;
    *str = line.substr(*str_pos, copy_size);
    *str_pos = (pos != std::string::npos) ? pos + FIELD_SEP.size() : std::string::npos;

    MFX_DEBUG_TRACE_I32(copy_size);
    MFX_DEBUG_TRACE_I32(*str_pos);
}

c2_status_t MfxC2ComponentStore::readConfigFile()
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t c2_res = C2_OK;
    std::string config_filename;

    config_filename.append(MFX_C2_CONFIG_FILE_PATH);
    config_filename.append("/");
    config_filename.append(MFX_C2_CONFIG_FILE_NAME);
    std::ifstream config_file(config_filename.c_str(), std::ifstream::in);

    if (config_file)
    {
        MFX_DEBUG_TRACE_S(config_filename.c_str());
        std::string line, str, name, module;

        while (std::getline(config_file, line))
        {
            MFX_DEBUG_TRACE_S(line.c_str());

            size_t pos = 0;

            // getting name
            MfxC2GetField(line, &str, &pos);
            if (str.empty()) continue; // line is empty or field is the last one
            name = str;
            MFX_DEBUG_TRACE_S(name.c_str());

            // getting module
            MfxC2GetField(line, &str, &pos);
            if (str.empty()) continue;
            module = str;
            MFX_DEBUG_TRACE_S(module.c_str());

            // getting optional flags
            MfxC2GetField(line, &str, &pos);
            int flags = 0;
            if (!str.empty())
            {
                MFX_DEBUG_TRACE_S(str.c_str());
                flags = strtol(str.c_str(), NULL, 16);
            }

            C2String media_type = xml_parser_.getMediaType(name.c_str());
            MFX_DEBUG_TRACE_S(media_type.c_str());

            MfxC2Component::CreateConfig config;
            config.flags = flags;
            config.dump_output = xml_parser_.dumpOutputEnabled(name.c_str());

            components_registry_.emplace(name, ComponentDesc(module.c_str(), media_type.c_str(), config));
        }
        config_file.close();
    }
    MFX_DEBUG_TRACE_I32(components_registry_.size());
    MFX_DEBUG_TRACE__android_c2_status_t(c2_res);
    return c2_res;
}

c2_status_t MfxC2ComponentStore::readXmlConfigFile()
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t c2_res = C2_OK;
    std::string config_filename = MFX_C2_CONFIG_XML_FILE_PATH;
    config_filename.append("/");
    config_filename.append(MFX_C2_CONFIG_XML_FILE_NAME);

    c2_res = xml_parser_.parseConfig(config_filename.c_str());

    MFX_DEBUG_TRACE__android_c2_status_t(c2_res);
    return c2_res;
}

void* MfxC2ComponentStore::loadModule(const std::string& name) {

    MFX_DEBUG_TRACE_FUNC;

    MFX_DEBUG_TRACE_S(name.c_str());

    void* result = nullptr;

    dlerror();
    result = dlopen(name.c_str(), RTLD_NOW);
    if(result == nullptr) {
        MFX_LOG_ERROR("Module %s load is failed: %s", name.c_str(), dlerror());
    }
    MFX_DEBUG_TRACE_P(result);
    return result;
}
