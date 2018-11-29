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

#define MAX_LINE_LENGTH 1024
#define FIELD_SEP " \t:"

using namespace android;

MfxC2ComponentStore* MfxC2ComponentStore::Create(c2_status_t* status) {

    MFX_DEBUG_TRACE_FUNC;

    MfxC2ComponentStore* store = new (std::nothrow)MfxC2ComponentStore();
    if (store != nullptr) {
        *status = store->readConfigFile();
        if (*status != C2_OK) {
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

                    MfxC2Component* mfx_component = (*create_func)(name.c_str(), it->second.flags_, &result);
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
            MFX_DEBUG_TRACE_S(info->name.c_str());
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
    static std::shared_ptr<C2ParamReflector> reflector =
        std::make_shared<MfxC2ParamReflector>();

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
 * FILED_SEP characters, Returns pointer to the beginning of the field and
 * its size.
 */
static void mfx_c2_get_field(const char *line, char **str, size_t *str_size)
{
    MFX_DEBUG_TRACE_FUNC;

    if (!line || !str || !str_size) return;

    MFX_DEBUG_TRACE_S(line);

    *str = (char*)line;
    for(; strchr(FIELD_SEP, **str) && (**str); ++(*str));
    if (**str)
    {
        char *p = *str;
        for(; !strchr(FIELD_SEP, *p) && (*p); ++p);
        *str_size = (p - *str);

        MFX_DEBUG_TRACE_I32(*str_size);
    }
    else
    {
        MFX_DEBUG_TRACE_MSG("field not found");
        *str = NULL;
        *str_size = 0;
        MFX_DEBUG_TRACE_I32(*str_size);
    }
}

c2_status_t MfxC2ComponentStore::readConfigFile()
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t c2_res = C2_OK;
    char config_filename[MFX_MAX_PATH] = {0};
    FILE* config_file = NULL;

#ifndef ANDROID
    { /* for Android we do not need this */
        char* home = getenv("HOME");
        if (home)
        {
            snprintf(config_filename, MFX_MAX_PATH, "%s/.%s", home, MFX_C2_CONFIG_FILE_NAME);
            config_file = fopen(config_filename, "r");
        }
    }
#endif
    if (!config_file)
    {
        snprintf(config_filename, MFX_MAX_PATH, "%s/%s", MFX_C2_CONFIG_FILE_PATH, MFX_C2_CONFIG_FILE_NAME);
        config_file = fopen(config_filename, "r");
    }
    if (config_file)
    {
        MFX_DEBUG_TRACE_S(config_filename);
        char line[MAX_LINE_LENGTH] = {0}, *str = NULL;
        char *name = NULL, *module = NULL, *str_flags = NULL;
        size_t line_length = 0, n = 0;//, i = 0;

        while (NULL != (str = fgets(line, MAX_LINE_LENGTH, config_file)))
        {
            line_length = n = strnlen(line, MAX_LINE_LENGTH);
            for(; n && strchr("\n\r", line[n-1]); --n)
            {
                line[n-1] = '\0';
                --line_length;
            }
            MFX_DEBUG_TRACE_S(line);
            MFX_DEBUG_TRACE_I32(line_length);

            // getting name
            mfx_c2_get_field(line, &str, &n);
            //if (!n && ((str+n+1 - line) < line_length)) continue;
            if (!n || !(str[n])) continue; // line is empty or field is the last one
            name = str;
            name[n] = '\0';
            MFX_DEBUG_TRACE_S(name);

            // getting module
            mfx_c2_get_field(str+n+1, &str, &n);
            if (!n) continue;
            module = str;
            module[n] = '\0';
            MFX_DEBUG_TRACE_S(module);

            // getting optional flags
            if ((size_t)(str+n+1 - line) < line_length)
            {
                mfx_c2_get_field(str+n+1, &str, &n);
                if (n)
                {
                    str_flags = str;
                    str_flags[n] = '\0';
                    MFX_DEBUG_TRACE_S(str_flags);
                }
            }
            int flags = (str_flags) ? strtol(str_flags, NULL, 16) : 0;

            components_registry_.emplace(std::string(name), ComponentDesc(module, flags));
        }
        fclose(config_file);
    }
    MFX_DEBUG_TRACE_I32(components_registry_.size());
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
