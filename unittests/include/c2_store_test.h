/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/
#include <C2.h>
#include <fstream>

namespace {

struct ComponentDesc
{
    const char* component_name;
    const char* module_name;
    int flags;
    c2_status_t creation_status;
};

ComponentDesc g_components[] = {
    { "C2.MockComponent.Enc", "libmfx_mock_c2_components.so", 0, C2_OK },
    { "C2.h264vd", "libmfx_c2_components_hw.so", 0, C2_OK },
    { "C2.h264ve", "libmfx_c2_components_hw.so", 0, C2_OK },
    { "C2.NonExistingComponent", "libmfx_mock_c2_components.so", 0, C2_NOT_FOUND },
};

inline bool PrepareConfFile()
{
    std::ofstream fileConf("/etc/mfx_c2_store.conf");

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

}
