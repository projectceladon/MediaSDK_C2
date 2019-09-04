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
#include "mfx_debug.h"
#include "c2_store_test.h"

#include <dlfcn.h>

using namespace android;

static bool ModuleInMemory(const std::string& module)
{
    void* handle = dlopen(module.c_str(), RTLD_NOLOAD);
    bool found = (handle != nullptr);
    if(found) {
        dlclose(handle);
    }
    return found;
}

static std::shared_ptr<C2ComponentStore> CreateComponentStore()
{
    std::shared_ptr<C2ComponentStore> result;

    bool conf_file_ready = PrepareConfFile();
    EXPECT_TRUE(conf_file_ready);
    bool conf_xml_file_ready = PrepareXmlConfFile();
    EXPECT_TRUE(conf_xml_file_ready);

    if (conf_file_ready || conf_xml_file_ready) {

        c2_status_t status = C2_OK;
        result.reset(MfxC2ComponentStore::Create(&status));
        EXPECT_EQ(status, C2_OK);
        EXPECT_TRUE(result);
        RestoreConfFile();
        RestoreXmlConfFile();
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
// A list should be equal to the list prepared by test in file /vendor/etc/mfx_c2_store.conf
// For this test the running device should be rooted and remounted to able to write to /etc dir.
TEST(MfxComponentStore, getComponents)
{
    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    auto components = componentStore->listComponents();

    EXPECT_EQ(components.size(), MFX_GET_ARRAY_SIZE(g_components));

    for (const auto& actual_comp : components) {

        bool found = false;
        for (const auto& expected_comp : g_components) {
            if (actual_comp->name == expected_comp.component_name) {
                EXPECT_NE(actual_comp->mediaType, "");
                EXPECT_EQ(actual_comp->mediaType, expected_comp.media_type);
                EXPECT_EQ(actual_comp->domain, DOMAIN_VIDEO);
                EXPECT_EQ(actual_comp->kind, expected_comp.kind);

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

// Assures that reflector provides desctiption about all parameters
// supported by components created by the store.
TEST(MfxComponentStore, getParamReflector)
{
    MFX_DEBUG_TRACE_FUNC;

    std::shared_ptr<C2ComponentStore> componentStore = GetCachedC2ComponentStore();
    ASSERT_NE(componentStore, nullptr);

    std::set<C2Param::CoreIndex> core_indices;

    for (const auto& component_desc : g_components) {
        std::shared_ptr<C2ComponentInterface> component_itf; // Create components to fill the reflector
        componentStore->createInterface(component_desc.component_name, &component_itf);
        if (component_itf) {
            std::vector<std::shared_ptr<C2ParamDescriptor>> params;
            c2_status_t status = component_itf->querySupportedParams_nb(&params);
            EXPECT_EQ(status, C2_OK);
            for (auto const& param : params) {
                core_indices.insert(param->index().coreIndex());
            }
        }
    }

    MFX_DEBUG_TRACE_STREAM("Supported params core indices: " << core_indices.size());
    EXPECT_GT(core_indices.size(), 0u);

    std::shared_ptr<C2ParamReflector> reflector = componentStore->getParamReflector();
    EXPECT_NE(reflector, nullptr);
    if (reflector) {
        for (C2Param::CoreIndex core_index : core_indices) {

            std::unique_ptr<C2StructDescriptor> descriptor = reflector->describe(core_index);
            EXPECT_NE(descriptor, nullptr);
        }
    }
}
