/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/
#include <C2.h>
#include <fstream>

#include "mfx_c2_defs.h"

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

#define RESET_LD_LIBRARY_PATH "LD_LIBRARY_PATH= "
// Test executables are launched with LD_LIBRARY_PATH=.:/system/lib/vndk-28 (or lib64 for 64-bit version)
// to have access to vndk libraries.
// Child processes inherit this environment variable and some of them got their dependencies broken.
// Actually 'cp' and 'mv' commands fail with message:
// CANNOT LINK EXECUTABLE "cp": "/system/lib/vndk-28/libselinux.so" is 32-bit instead of 64-bit
// That's why 'cp' and 'mv' need LD_LIBRARY_PATH reset.

inline bool PrepareConfFile()
{
    const char* backup_cmd_line = "cd " MFX_C2_CONFIG_FILE_PATH "; "
        "if [ -f " MFX_C2_CONFIG_FILE_NAME " ]; "
            "then " RESET_LD_LIBRARY_PATH "cp " MFX_C2_CONFIG_FILE_NAME " " MFX_C2_CONFIG_FILE_NAME ".bak; fi";
    std::system(backup_cmd_line);

    std::ofstream fileConf(MFX_C2_CONFIG_FILE_PATH "/" MFX_C2_CONFIG_FILE_NAME);

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

inline void RestoreConfFile()
{
    const char* restore_cmd_line = "cd " MFX_C2_CONFIG_FILE_PATH "; "
        "if [ -f " MFX_C2_CONFIG_FILE_NAME ".bak ]; "
            "then " RESET_LD_LIBRARY_PATH " mv " MFX_C2_CONFIG_FILE_NAME ".bak " MFX_C2_CONFIG_FILE_NAME "; fi";
    std::system(restore_cmd_line);
}

#undef RESET_LD_LIBRARY_PATH

}
