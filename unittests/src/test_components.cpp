/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "test_components.h"
#include "gtest_emulation.h"
#include <sstream>

BinaryWriter::BinaryWriter(const std::vector<std::string>& folders, const std::string& name)
{
    if(enabled_) {

        std::stringstream full_name;
        full_name << "./";

        for(const std::string& folder : folders) {
            full_name << folder;

            bool dir_exists = false;

            struct stat info;

            if (stat(full_name.str().c_str(), &info) == 0) {
                dir_exists = (info.st_mode & S_IFDIR) != 0;
            }

            mkdir(full_name.str().c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);

            full_name << "/";
        }

        full_name << name;
        stream_.open(full_name.str().c_str(), std::fstream::trunc | std::fstream::binary);
    }
}

bool BinaryWriter::enabled_ = false;

GTestBinaryWriter::GTestBinaryWriter(const std::string& name)
    : BinaryWriter(GetTestFolders(), name)
{

}

std::vector<std::string> GTestBinaryWriter::GetTestFolders()
{
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    return { test_info->test_case_name(), test_info->name() };
}
