/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_defs.h"
#include "gtest_emulation.h"
#include "test_components.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"

using namespace android;

namespace {
    struct ComponentDesc
    {
        const char* component_name;
        int flags;
        status_t creation_status;
    };
}

static ComponentDesc g_components_desc[] = {
    { "C2.h264vd", 0, C2_OK },
    { "C2.NonExistingDecoder", 0, C2_NOT_FOUND },
};

static const ComponentDesc* GetComponentDesc(const std::string& component_name)
{
    const ComponentDesc* result = nullptr;
    for(const auto& desc : g_components_desc) {
        if(component_name == desc.component_name) {
            result = &desc;
            break;
        }
    }
    return result;
}

static std::map<std::string, std::shared_ptr<MfxC2Component>>& GetComponentsCache()
{
    static std::map<std::string, std::shared_ptr<MfxC2Component>> g_components;
    return g_components;
}

static std::shared_ptr<MfxC2Component> GetCachedComponent(const char* name)
{
    std::shared_ptr<MfxC2Component> result;
    auto& components_cache = GetComponentsCache(); // auto& is needed to have ref not a copy of cache

    auto it = components_cache.find(name);
    if(it != components_cache.end()) {
        result = it->second;
    }
    else {
        const ComponentDesc* desc = GetComponentDesc(name);
        ASSERT_NE(desc, nullptr);

        MfxC2Component* mfx_component;
        status_t status = MfxCreateC2Component(name, desc->flags, &mfx_component);
        EXPECT_EQ(status, desc->creation_status);
        if(desc->creation_status == C2_OK) {
            EXPECT_NE(mfx_component, nullptr);
            result = std::shared_ptr<MfxC2Component>(mfx_component);

            components_cache.emplace(name, result);
        }
    }
    return result;
}

// Assures that all decoding components might be successfully created.
// NonExistingDecoder cannot be created and C2_NOT_FOUND error is returned.
TEST(MfxDecoderComponent, Create)
{
    for(const auto& desc : g_components_desc) {

        std::shared_ptr<MfxC2Component> decoder = GetCachedComponent(desc.component_name);

        EXPECT_EQ(decoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;
    }
}

// Checks that all successfully created decoding components expose C2ComponentInterface
// and return correct information once queried (component name).
TEST(MfxDecoderComponent, intf)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc& desc, C2CompPtr, C2CompIntfPtr comp_intf) {

        EXPECT_EQ(comp_intf->getName(), desc.component_name);
    } );
}

// Checks the correctness of all decoding components state machine.
// The component should be able to start from STOPPED (initial) state,
// stop from RUNNING state. Otherwise, C2_BAD_STATE should be returned.
TEST(MfxDecoderComponent, State)
{
    ForEveryComponent<ComponentDesc>(g_components_desc, GetCachedComponent,
        [] (const ComponentDesc&, C2CompPtr comp, C2CompIntfPtr) {

        status_t sts = C2_OK;

        sts = comp->start();
        EXPECT_EQ(sts, C2_OK);

        sts = comp->start();
        EXPECT_EQ(sts, C2_BAD_STATE);

        sts = comp->stop();
        EXPECT_EQ(sts, C2_OK);

        sts = comp->stop();
        EXPECT_EQ(sts, C2_BAD_STATE);
    } );
}
