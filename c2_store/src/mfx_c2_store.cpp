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

#include "mfx_c2_store.h"
#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_component.h"
#include <cutils/properties.h>

#include <dlfcn.h>
#include <fstream>
#include <sys/stat.h>

static const std::string FIELD_SEP = " : ";

using namespace android;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "MfxC2ComponentStore"

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

    char szVendorIntelVideoCodec[PROPERTY_VALUE_MAX] = {'\0'};
    if(property_get("vendor.intel.video.codec", szVendorIntelVideoCodec, NULL) > 0 ) {
        if (strncmp(szVendorIntelVideoCodec, "software", PROPERTY_VALUE_MAX) == 0 ) {
            ALOGI("Property vendor.intel.video.codec is %s in auto_hal.in and will not load hardware codec plugin", szVendorIntelVideoCodec);
            return C2_NOT_FOUND;
        }
    }

    if(component != nullptr) {

        auto it = m_componentsRegistry_.find(name);
        if(it != m_componentsRegistry_.end()) {

            auto dso_deleter = [] (void* handle) { dlclose(handle); };
            std::unique_ptr<void, decltype(dso_deleter)> dso(loadModule(it->second.dso_name_), dso_deleter);
            if(dso != nullptr) {

                CreateMfxC2ComponentFunc* create_func =
                    reinterpret_cast<CreateMfxC2ComponentFunc*>(dlsym(dso.get(), CREATE_MFX_C2_COMPONENT_FUNC_NAME));
                if(create_func != nullptr) {

                    std::shared_ptr<C2ReflectorHelper> reflector;
                    {
                        std::lock_guard<std::mutex> lock(m_reflectorMutex);
                        reflector = m_reflector; // safe copy
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
    c2_status_t result = createComponent(std::move(name), &component);

    if(result == C2_OK) {
        *interface = component->intf();
    }
    return result;
}

std::vector<std::shared_ptr<const C2Component::Traits>> MfxC2ComponentStore::listComponents() {

    MFX_DEBUG_TRACE_FUNC;
    std::vector<std::shared_ptr<const C2Component::Traits>> result;

    try {
        for(const auto& it_pair : m_componentsRegistry_ ) {
            std::unique_ptr<C2Component::Traits> traits = std::make_unique<C2Component::Traits>();
            traits->name = it_pair.first;
            traits->domain = DOMAIN_VIDEO;
            traits->kind = it_pair.second.kind_;
            traits->mediaType = it_pair.second.media_type_;

            MFX_DEBUG_TRACE_S(traits->name.c_str());
            MFX_DEBUG_TRACE_S(traits->mediaType.c_str());
            result.push_back(std::move(traits));
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
    std::shared_ptr<C2ParamReflector> reflector;
    {
        std::lock_guard<std::mutex> lock(m_reflectorMutex);
        reflector = dynamic_pointer_cast<C2ParamReflector>(m_reflector); // safe copy
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
    if (config_filename.compare(config_filename.size() - 5, 5, ".conf") == 0) {
        char codecsVariant[PROPERTY_VALUE_MAX] = {'\0'};
        if(property_get("ro.vendor.media.target_variant_platform", codecsVariant, NULL) > 0 ) {
            std::string variant_config_file = config_filename;
            variant_config_file.insert(variant_config_file.size() - 5, codecsVariant);
            struct stat fileStat;
            if (stat(variant_config_file.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                config_filename = variant_config_file;
                MFX_DEBUG_TRACE_STREAM("Changing config_filename to: " << config_filename.c_str());
            }
        }
    }

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

            C2String media_type = m_xmlParser.getMediaType(name.c_str());
            MFX_DEBUG_TRACE_S(media_type.c_str());

            C2Component::kind_t kind = m_xmlParser.getKind(name.c_str());

            MfxC2Component::CreateConfig config;
            config.flags = flags;
            config.dump_output = m_xmlParser.dumpOutputEnabled(name.c_str());
            config.concurrent_instances = m_xmlParser.getConcurrentInstances(name.c_str());

            m_componentsRegistry_.emplace(name, ComponentDesc(module.c_str(), media_type.c_str(), kind, config));
        }
        config_file.close();
    }
    MFX_DEBUG_TRACE_I32(m_componentsRegistry_.size());
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
    MFX_DEBUG_TRACE_S(config_filename.c_str());
    if (config_filename.compare(config_filename.size() - 4, 4, ".xml") == 0) {
        char codecsVariant[PROPERTY_VALUE_MAX] = {'\0'};
        if(property_get("ro.vendor.media.target_variant_platform", codecsVariant, NULL) > 0 ) {
            std::string variant_config_file = config_filename;
            variant_config_file.insert(variant_config_file.size() - 4, codecsVariant);
            struct stat fileStat;
            if (stat(variant_config_file.c_str(), &fileStat) == 0 && S_ISREG(fileStat.st_mode)) {
                config_filename = variant_config_file;
                MFX_DEBUG_TRACE_STREAM("Changing xml config_filename to: " << config_filename.c_str());
            }
        }
    }
    c2_res = m_xmlParser.parseConfig(config_filename.c_str());

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
