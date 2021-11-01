// Copyright (c) 2017-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <C2.h>
#include <C2Component.h>
#include <fstream>

#include "mfx_c2_defs.h"

namespace {

struct ComponentDesc
{
    const char* component_name;
    const char* media_type;
    C2Component::kind_t kind;
    const char* module_name;
    int flags;
    c2_status_t creation_status;
};

ComponentDesc g_components[] = {
    { "c2.intel.mock.encoder", "video/mock", KIND_ENCODER, "libmfx_mock_c2_components.so", 0, C2_OK },
    { "c2.intel.avc.decoder", "video/avc", KIND_DECODER, "libmfx_c2_components_hw.so", 0, C2_OK },
    { "c2.intel.avc.encoder", "video/avc", KIND_ENCODER, "libmfx_c2_components_hw.so", 0, C2_OK },
    { "c2.intel.missing.encoder", "video/missed", KIND_ENCODER, "libmfx_mock_c2_components.so", 0, C2_NOT_FOUND },
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

inline bool PrepareXmlConfFile()
{
    const char* backup_cmd_line = "cd " MFX_C2_CONFIG_XML_FILE_PATH "; "
        "if [ -f " MFX_C2_CONFIG_XML_FILE_NAME " ]; "
            "then " RESET_LD_LIBRARY_PATH "cp " MFX_C2_CONFIG_XML_FILE_NAME " " MFX_C2_CONFIG_XML_FILE_NAME ".bak; fi";
    std::system(backup_cmd_line);

    std::string config_filename;
    config_filename.append(MFX_C2_CONFIG_XML_FILE_PATH);
    config_filename.append("/");
    config_filename.append(MFX_C2_CONFIG_XML_FILE_NAME);

    std::ofstream config_file(config_filename.c_str(), std::ifstream::out);

    if (!config_file)
        return false;

    std::string decoders_str;
    std::string encoders_str;
    for(const auto& component : g_components) {
        std::string line = "        <MediaCodec name=\"";
        line.append(component.component_name);
        line.append("\" type=\"");
        line.append(component.media_type);
        line.append("\">\n");
        line.append("        </MediaCodec>\n");
        if(strstr(component.component_name, "decoder")) {
            decoders_str.append(line);
        } else if (strstr(component.component_name, "encoder")) {
            encoders_str.append(line);
        }
    }


    config_file << "<Included>\n";
    config_file << "    <Decoders>\n";
    config_file << decoders_str.c_str();
    config_file << "    </Decoders>\n";
    config_file << "\n";
    config_file << "    <Encoders>\n";
    config_file << encoders_str.c_str();
    config_file << "    </Encoders>\n";
    config_file << "</Included>\n";

    config_file.close();
    return true;
}

inline void RestoreConfFile()
{
    const char* restore_cmd_line = "cd " MFX_C2_CONFIG_FILE_PATH "; "
        "if [ -f " MFX_C2_CONFIG_FILE_NAME ".bak ]; "
            "then " RESET_LD_LIBRARY_PATH " mv " MFX_C2_CONFIG_FILE_NAME ".bak " MFX_C2_CONFIG_FILE_NAME "; fi";
    std::system(restore_cmd_line);
}

inline void RestoreXmlConfFile()
{
    const char* restore_cmd_line = "cd " MFX_C2_CONFIG_XML_FILE_PATH "; "
        "if [ -f " MFX_C2_CONFIG_XML_FILE_NAME ".bak ]; "
            "then " RESET_LD_LIBRARY_PATH " mv " MFX_C2_CONFIG_XML_FILE_NAME ".bak " MFX_C2_CONFIG_XML_FILE_NAME "; fi";
    std::system(restore_cmd_line);
}

#undef RESET_LD_LIBRARY_PATH

}
