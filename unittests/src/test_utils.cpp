/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "test_utils.h"

#include <string>
#include <functional>
#include <vector>
#include <iostream>

std::ostringstream g_failures_stream;

std::vector<TestRegistration>& GetTestsRegistry()
{
    static std::vector<TestRegistration> g_tests_registry;
    return g_tests_registry;
}

void RegisterTest(const TestRegistration& registration)
{
    GetTestsRegistry().push_back(registration);
}

TestRegistration::TestRegistration(const std::string& test_case_name, const std::string& test_name, TestFunction* func):
    test_case_name_(test_case_name), test_name_(test_name), func_(func)
{
    RegisterTest(*this);
}

int RUN_ALL_TESTS()
{
    int failed_tests = 0;

    for(const auto& test : GetTestsRegistry()) {

        std::cout << test.test_case_name_ << ":" << test.test_name_;
        g_failures_stream = std::ostringstream();

        try {
            test.func_();

            if(!g_failures_stream.str().empty()) {
                std::cout << " failed " << g_failures_stream.str() << std::endl;
                ++failed_tests;
            }
            else {
                std::cout << " succeeded" << std::endl;
            }
        }
        catch(const std::ostringstream& str) {
            std::cout << " failed " << str.str() << g_failures_stream.str() << std::endl;
            ++failed_tests;
        }
    }
    if(failed_tests != 0) {
        std::cout << std::endl << failed_tests << " of " <<
            GetTestsRegistry().size() << " test(s) FAILED" << std::endl;
    }
    else {
        std::cout << std::endl << GetTestsRegistry().size() << " test(s) SUCCEEDED" << std::endl;
    }
    return failed_tests != 0;
}
