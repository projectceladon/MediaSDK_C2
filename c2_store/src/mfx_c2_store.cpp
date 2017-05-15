/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2016 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_c2_store.cpp

Defined functions:

Defined help functions:

*********************************************************************************/
#include "mfx_c2_store.h"
#include "mfx_c2.h"
#include "mfx_c2_defs.h"

#define EXPORT __attribute__((visibility("default")))

#define MAX_LINE_LENGTH 1024
#define FIELD_SEP " \t:"

using namespace android;

EXPORT status_t GetC2ComponentStore(std::shared_ptr<C2ComponentStore>* const componentStore) {

    status_t creationStatus = C2_OK;
    static std::shared_ptr<MfxC2ComponentStore> g_componentStore =
        std::shared_ptr<MfxC2ComponentStore>(MfxC2ComponentStore::Create(&creationStatus));
    *componentStore = g_componentStore;
    return creationStatus;
}

MfxC2ComponentStore* MfxC2ComponentStore::Create(status_t* status) {

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
    return store;
}

status_t MfxC2ComponentStore::createComponent(C2String name, std::shared_ptr<C2Component>* const component) {
    (void)name;
    (void)component;
    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2ComponentStore::createInterface(C2String name, std::shared_ptr<C2ComponentInterface>* const interface) {
    (void)name;
    (void)interface;
    return C2_NOT_IMPLEMENTED;
}

std::vector<std::unique_ptr<const C2ComponentInfo>> MfxC2ComponentStore::getComponents() {
    return std::vector<std::unique_ptr<const C2ComponentInfo>>();
}

status_t MfxC2ComponentStore::copyBuffer(std::shared_ptr<C2GraphicBuffer> src, std::shared_ptr<C2GraphicBuffer> dst) {
    (void)src;
    (void)dst;
    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2ComponentStore::query_nb(
        const std::vector<C2Param * const> &stackParams,
        const std::vector<C2Param::Index> &heapParamIndices,
        std::vector<std::unique_ptr<C2Param>>*const heapParams) {

    (void)stackParams;
    (void)heapParamIndices;
    (void)heapParams;
    return C2_NOT_IMPLEMENTED;
}

status_t MfxC2ComponentStore::config_nb(
        const std::vector<C2Param * const> &params,
        std::list<std::unique_ptr<C2SettingResult>>*const failures) {

    (void)params;
    (void)failures;
    return C2_NOT_IMPLEMENTED;
}

/* Searches in the given line field which is separated from other lines by
 * FILED_SEP characters, Returns pointer to the beginning of the field and
 * its size.
 */
static void mfx_c2_get_field(const char *line, char **str, size_t *str_size)
{
    MFX_C2_AUTO_TRACE_FUNC();

    if (!line || !str || !str_size) return;

    MFX_C2_AUTO_TRACE_S(line);

    *str = (char*)line;
    for(; strchr(FIELD_SEP, **str) && (**str); ++(*str));
    if (**str)
    {
        char *p = *str;
        for(; !strchr(FIELD_SEP, *p) && (*p); ++p);
        *str_size = (p - *str);

        MFX_C2_AUTO_TRACE_I32(*str_size);
    }
    else
    {
        MFX_C2_AUTO_TRACE_MSG("field not found");
        *str = NULL;
        *str_size = 0;
        MFX_C2_AUTO_TRACE_I32(*str_size);
    }
}

status_t MfxC2ComponentStore::readConfigFile()
{
    MFX_C2_AUTO_TRACE_FUNC();
    status_t c2_res = C2_OK;
    char config_filename[MFX_C2_MAX_PATH] = {0};
    FILE* config_file = NULL;

#ifndef ANDROID
    { /* for Android we do not need this */
        char* home = getenv("HOME");
        if (home)
        {
            snprintf(config_filename, MFX_C2_MAX_PATH, "%s/.%s", home, MFX_C2_CONFIG_FILE_NAME);
            config_file = fopen(config_filename, "r");
        }
    }
#endif
    if (!config_file)
    {
        snprintf(config_filename, MFX_C2_MAX_PATH, "%s/%s", MFX_C2_CONFIG_FILE_PATH, MFX_C2_CONFIG_FILE_NAME);
        config_file = fopen(config_filename, "r");
    }
    if (config_file)
    {
        char line[MAX_LINE_LENGTH] = {0}, *str = NULL;
        char *name = NULL, *module = NULL;
        size_t line_length = 0, n = 0;//, i = 0;

        while (NULL != (str = fgets(line, MAX_LINE_LENGTH, config_file)))
        {
            line_length = n = strnlen(line, MAX_LINE_LENGTH);
            for(; n && strchr("\n\r", line[n-1]); --n)
            {
                line[n-1] = '\0';
                --line_length;
            }
            MFX_C2_AUTO_TRACE_S(line);
            MFX_C2_AUTO_TRACE_I32(line_length);

            // getting name
            mfx_c2_get_field(line, &str, &n);
            //if (!n && ((str+n+1 - line) < line_length)) continue;
            if (!n || !(str[n])) continue; // line is empty or field is the last one
            name = str;
            name[n] = '\0';
            MFX_C2_AUTO_TRACE_S(name);

            // getting module
            mfx_c2_get_field(str+n+1, &str, &n);
            if (!n) continue;
            module = str;
            module[n] = '\0';
            MFX_C2_AUTO_TRACE_S(value);

            // getting optional flags, roles left for further implementation
            components_registry_.emplace_back(name, module);
        }
        fclose(config_file);
    }
    MFX_C2_AUTO_TRACE_P(components_registry_);
    MFX_C2_AUTO_TRACE_U32(c2_res);
    return c2_res;
}
