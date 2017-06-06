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
#include <dlfcn.h>

#include "mfx_c2_defs.h"
#include "test_utils.h"
#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_c2_mock_component.h"

using namespace android;

#define MOCK_COMPONENT "C2.MockComponent"


TEST(MfxMockComponent, Create)
{
    int flags = 0;
    MfxC2Component* c_mfx_component;
    status_t result = MfxCreateC2Component(MOCK_COMPONENT, flags, &c_mfx_component);
    std::shared_ptr<MfxC2Component> mfx_component(c_mfx_component);
    EXPECT_EQ(result, C2_OK);
    EXPECT_NE(mfx_component, nullptr);
}

TEST(MfxMockComponent, intf)
{
    int flags = 0;
    MfxC2Component* c_mfx_component;
    status_t result = MfxCreateC2Component(MOCK_COMPONENT, flags, &c_mfx_component);
    std::shared_ptr<MfxC2Component> mfx_component(c_mfx_component);

    EXPECT_NE(mfx_component, nullptr);
    if(mfx_component != nullptr) {
        std::shared_ptr<C2Component> c2_component = mfx_component;
        std::shared_ptr<C2ComponentInterface> c2_component_intf = c2_component->intf();

        EXPECT_NE(c2_component_intf, nullptr);
        if(c2_component_intf != nullptr) {
            EXPECT_EQ(c2_component_intf->getName(), MOCK_COMPONENT);
        }
    }
}
