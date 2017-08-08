/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_defs.h"
#include "gtest_emulation.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"

using namespace android;

struct ComponentDesc
{
    const char* component_name;
    int flags;
    status_t creation_status;
};

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

static std::shared_ptr<MfxC2Component> GetCachedComponent(const char* name)
{
    static std::map<std::string, std::shared_ptr<MfxC2Component>> g_components;

    std::shared_ptr<MfxC2Component> result;

    auto it = g_components.find(name);
    if(it != g_components.end()) {
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

            g_components.emplace(name, result);
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
    for(const auto& desc : g_components_desc) {
        std::shared_ptr<MfxC2Component> decoder = GetCachedComponent(desc.component_name);
        EXPECT_EQ(decoder != nullptr, desc.creation_status == C2_OK) << " for " << desc.component_name;;

        if(decoder != nullptr) {
            std::shared_ptr<C2Component> c2_component = decoder;
            std::shared_ptr<C2ComponentInterface> c2_component_intf = c2_component->intf();

            EXPECT_NE(c2_component_intf, nullptr);

            if(c2_component_intf != nullptr) {
                EXPECT_EQ(c2_component_intf->getName(), desc.component_name);
            }
        }
    }
}
