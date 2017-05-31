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

using namespace android;

struct ComponentDesc
{
    const char* component_name;
    int flags;
    status_t creation_status;
};

ComponentDesc g_components[] = {
    { "C2.MockComponent", 0, C2_OK },
    { "C2.h264ve", 0, C2_NOT_FOUND },
};

static bool PrepareConfFile()
{
#ifndef ANDROID
    std::string home = std::getenv("HOME");
#else
    std::string home = "/etc";
#endif
    std::ofstream fileConf(home + "/mfx_c2_store.conf");

    for(const auto& component : g_components) {
        fileConf << component.component_name << " : libmfx_c2_components.so";
        if(component.flags != 0) {
            fileConf << " : " << component.flags;
        }
        fileConf << std::endl;
        fileConf.close();
    }
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

    size_t existing_count = 0;
    for(const auto& component : g_components) {
        if(component.creation_status == C2_OK)
            ++existing_count;
    }

    EXPECT_EQ(components.size(), existing_count);

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

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    return RUN_ALL_TESTS();
}
