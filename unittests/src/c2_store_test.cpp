/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <gtest/gtest.h>

#include "mfx_c2_store.h"
#include "mfx_defs.h"

#include <dlfcn.h>

using namespace android;

struct ComponentDesc
{
    const char* component_name;
    const char* module_name;
    int flags;
    c2_status_t creation_status;
};

ComponentDesc g_components[] = {
    { "C2.MockComponent.Enc", "libmfx_mock_c2_components.so", 0, C2_OK },
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

static std::shared_ptr<C2ComponentStore> CreateComponentStore()
{
    std::shared_ptr<C2ComponentStore> result;

    bool conf_file_ready = PrepareConfFile();
    EXPECT_TRUE(conf_file_ready);

    if (conf_file_ready) {

        c2_status_t status = C2_OK;
        result.reset(MfxC2ComponentStore::Create(&status));
        EXPECT_EQ(status, C2_OK);
        EXPECT_TRUE(result);
    }
    return result;
}

// this function creates component store and keeps for subsequent usage
static std::shared_ptr<C2ComponentStore> GetCachedC2ComponentStore()
{
    static std::shared_ptr<C2ComponentStore> g_store = CreateComponentStore();
    return g_store;
}

// Tests if the component store can be created by itself.
TEST(MfxComponentStore, Create)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    EXPECT_NE(componentStore, nullptr);
}

// Tests if store returns correct list of supported components.
// A list should be equal to the list prepared by test in file /etc/mfx_c2_store.conf
// For this test the running device should be rooted and remounted to able to write to /etc dir.
TEST(MfxComponentStore, getComponents)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    auto components = componentStore->listComponents();

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

// Tests if all components from the list could be created.
// Creation is made with C2ComponentStore::createComponent.
// Then created component returned C2ComponentInterface to check component name.
// A module loaded into memory is checked as well.
TEST(MfxComponentStore, createComponent)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    for(const auto& component_desc : g_components) {
        std::shared_ptr<C2Component> component;
        c2_status_t status = componentStore->createComponent(component_desc.component_name, &component);
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
                // It might be useful to check that component_desc.module_name is unloaded from memory
                // but system can keep it for various reasons.
            }
       }
    }
}

// Tests if all components from the list could be created via C2ComponentStore::createInterface.
// Test verifies that correct DSO is loaded into memory.
// Also test checks that component returns valid information via interface (b.e., returns name).
TEST(MfxComponentStore, createInterface)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    for(const auto& component_desc : g_components) {
        std::shared_ptr<C2ComponentInterface> component_itf;
        c2_status_t status = componentStore->createInterface(component_desc.component_name, &component_itf);
        EXPECT_EQ(status, component_desc.creation_status);

        if(component_desc.creation_status == C2_OK) {
            EXPECT_NE(component_itf, nullptr);
            EXPECT_EQ(ModuleInMemory(component_desc.module_name), true);

            if(component_itf != nullptr) {
                EXPECT_EQ(component_itf->getName(), component_desc.component_name);

                component_itf = nullptr;
                // It might be useful to check that component_desc.module_name is unloaded from memory
                // but system can keep it for various reasons.
            }
        }
    }
}

// Checks C2ComponentStore::copyBuffer returns C2_OMITTED for now.
TEST(MfxComponentStore, copyBuffer)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    std::shared_ptr<C2GraphicBuffer> src;
    std::shared_ptr<C2GraphicBuffer> dst;

    c2_status_t status = componentStore->copyBuffer(src, dst);
    EXPECT_EQ(status, C2_OMITTED);
}

// Checks C2ComponentStore::query_sm (query global store parameter)
// returns C2_OMITTED for now.
TEST(MfxComponentStore, query_sm)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    std::vector<C2Param*> stackParams;
    std::vector<C2Param::Index> heapParamIndices;
    std::vector<std::unique_ptr<C2Param>> heapParams;

    c2_status_t status = componentStore->query_sm(stackParams, heapParamIndices, &heapParams);
    EXPECT_EQ(status, C2_OMITTED);
}

// Checks C2ComponentStore::config_sm (set global store parameter)
// returns C2_OMITTED for now.
TEST(MfxComponentStore, config_sm)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    std::vector<C2Param*> params;
    std::vector<std::unique_ptr<C2SettingResult>> failures;

    c2_status_t status = componentStore->config_sm(params, &failures);
    EXPECT_EQ(status, C2_OMITTED);
}
