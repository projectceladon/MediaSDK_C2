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

    GTestBinaryWriter::Enable(dump_output);

    return RUN_ALL_TESTS();
}
