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
    { "c2.intel.mock.encoder", "libmfx_mock_c2_components.so", 0, C2_OK },
    { "c2.intel.avc.decoder", "libmfx_c2_components_hw.so", 0, C2_OK },
    { "c2.intel.avc.encoder", "libmfx_c2_components_hw.so", 0, C2_OK },
    { "c2.intel.missing.encoder", "libmfx_mock_c2_components.so", 0, C2_NOT_FOUND },
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
