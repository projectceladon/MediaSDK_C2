/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstdlib>

#include "mfx_c2.h"
#include "mfx_defs.h"
#include "test_utils.h"

#include <dlfcn.h>

using namespace android;

struct ComponentDesc
{
    const char* component_name;
    const char* module_name;
    int flags;
    status_t creation_status;
};

ComponentDesc g_components[] = {
    { "C2.MockComponent", "libmfx_mock_c2_components.so", 0, C2_OK },
    { "C2.h264ve", "libmfx_c2_components_hw.so", 0, C2_OK },
    { "C2.NonExistingComponent", "libmfx_c2_components_pure.so", 0, C2_NOT_FOUND },
};

static bool ModuleInMemory(const std::string& module)
{
    void* handle = dlopen(module.c_str(), RTLD_NOLOAD);
    bool found = (handle != nullptr);
    if(found) {
        dlclose(handle);
    }
    return found;
}

static bool PrepareConfFile()
{
#ifndef ANDROID
    std::string home = std::getenv("HOME");
#else
    std::string home = "/etc";
#endif
    std::ofstream fileConf(home + "/mfx_c2_store.conf");

    for(const auto& component : g_components) {
        fileConf << component.component_name << " : " << component.module_name;
        if(component.flags != 0) {
            fileConf << " : " << component.flags;
        }
        fileConf << std::endl;
    }
    fileConf.close();
    return true;
}

// this function creates component store and keeps for subsequent usage
static status_t GetCachedC2ComponentStore(std::shared_ptr<android::C2ComponentStore>* store)
{
    static bool conf_file_ready = PrepareConfFile();
    static std::shared_ptr<android::C2ComponentStore> g_store;
    static status_t g_creation_status = GetC2ComponentStore(&g_store);

    ASSERT_NE(g_store, nullptr);
    *store = g_store;
    return g_creation_status;
}

TEST(MfxComponentStore, Create)
{
    std::shared_ptr<android::C2ComponentStore> componentStore;
    status_t status = GetCachedC2ComponentStore(&componentStore);

    EXPECT_EQ(status, C2_OK);
}

TEST(MfxComponentStore, getComponents)
{
    std::shared_ptr<android::C2ComponentStore> componentStore;
    GetCachedC2ComponentStore(&componentStore);

    auto components = componentStore->getComponents();

    EXPECT_EQ(components.size(), MFX_GET_ARRAY_SIZE(g_components));

    for(size_t i = 0; i < components.size(); i++) {
        std::string actual_name = components[i]->name;

        bool found = false;
        for(const auto& component : g_components) {
            if(actual_name == component.component_name) {
                found = true;
                break;
            }
        }

        EXPECT_EQ(found, true);
    }
}

TEST(MfxComponentStore, createComponent)
{
    std::shared_ptr<android::C2ComponentStore> componentStore;
    GetCachedC2ComponentStore(&componentStore);

    for(const auto& component_desc : g_components) {
        std::shared_ptr<C2Component> component;
        status_t status = componentStore->createComponent(component_desc.component_name, &component);
        EXPECT_EQ(status, component_desc.creation_status);
        if(component_desc.creation_status == C2_OK) {
            EXPECT_NE(component, nullptr);

            if(component != nullptr) {

                EXPECT_EQ(ModuleInMemory(component_desc.module_name), true) <<
                    component_desc.module_name << " for " << component_desc.component_name;

                std::shared_ptr<C2ComponentInterface> component_itf = component->intf();
                EXPECT_NE(component_itf, nullptr);
                if(component_itf != nullptr) {
                    EXPECT_EQ(component_itf->getName(), component_desc.component_name);
                }

                component_itf = nullptr;
                component = nullptr;
                EXPECT_EQ(ModuleInMemory(component_desc.module_name), false)
                    << " module:" << component_desc.module_name;
            }
       }
    }
}

TEST(MfxComponentStore, createInterface)
{
    std::shared_ptr<android::C2ComponentStore> componentStore;
    GetCachedC2ComponentStore(&componentStore);

    for(const auto& component_desc : g_components) {
        std::shared_ptr<C2ComponentInterface> component_itf;
        status_t status = componentStore->createInterface(component_desc.component_name, &component_itf);
        EXPECT_EQ(status, component_desc.creation_status);

        if(component_desc.creation_status == C2_OK) {
            EXPECT_NE(component_itf, nullptr);
            EXPECT_EQ(ModuleInMemory(component_desc.module_name), true);

            if(component_itf != nullptr) {
                EXPECT_EQ(component_itf->getName(), component_desc.component_name);

                component_itf = nullptr;
                EXPECT_EQ(ModuleInMemory(component_desc.module_name), false)
                    << " module:" << component_desc.module_name;
            }
        }
    }
}

TEST(MfxComponentStore, copyBuffer)
{
    std::shared_ptr<android::C2ComponentStore> componentStore;
    GetCachedC2ComponentStore(&componentStore);

    std::shared_ptr<C2GraphicBuffer> src;
    std::shared_ptr<C2GraphicBuffer> dst;

    status_t status = componentStore->copyBuffer(src, dst);
    EXPECT_EQ(status, C2_NOT_IMPLEMENTED);
}

TEST(MfxComponentStore, query_nb)
{
    std::shared_ptr<android::C2ComponentStore> componentStore;
    GetCachedC2ComponentStore(&componentStore);

    std::vector<C2Param* const> stackParams;
    std::vector<C2Param::Index> heapParamIndices;
    std::vector<std::unique_ptr<C2Param>> heapParams;

    status_t status = componentStore->query_nb(stackParams, heapParamIndices, &heapParams);
    EXPECT_EQ(status, C2_NOT_IMPLEMENTED);
}

TEST(MfxComponentStore, config_nb)
{
    std::shared_ptr<android::C2ComponentStore> componentStore;
    GetCachedC2ComponentStore(&componentStore);

    std::vector<C2Param* const> params;
    std::list<std::unique_ptr<C2SettingResult>> failures;

    status_t status = componentStore->config_nb(params, &failures);
    EXPECT_EQ(status, C2_NOT_IMPLEMENTED);
}
