/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include <gtest/gtest.h>
#include "test_components.h"

#include <iostream>

int main(int argc, char** argv) {

    ::testing::InitGoogleTest(&argc, argv);

    // gtest doesn't provide any support to custom command line options,
    // they should be processed after gtest processed.
    bool dump_output = false;

    for(int i = 1; i < argc; ++i) {

        std::string arg(argv[i]);

        if(arg == "--dump-output") {
            dump_output = true;
        } else {
            std::cout << "Unexpected argument: " << arg << std::endl;
        }
    }

    BinaryWriter::Enable(dump_output);

    return RUN_ALL_TESTS();
}
