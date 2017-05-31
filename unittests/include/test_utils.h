/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <string>
#include <sstream>

#define TEST(test_case_name, test_name) \
    void test_case_name##test_name(); \
    static TestRegistration g_##test_case_name##test_name (#test_case_name, #test_name, &test_case_name##test_name); \
    void test_case_name##test_name()

#define ASSERT_NE(p0, p1) \
    if(!((p0) != (p1))) \
        throw std::ostringstream() << "Assertion failed: " #p0 " != " #p1 " "

#define ASSERT_EQ(p0, p1) \
    if(!((p0) == (p1))) \
        throw std::ostringstream() << "Assertion failed: " #p0 " == " #p1 " "

#define ASSERT_GE(p0, p1) \
    if(!((p0) >= (p1))) \
        throw std::ostringstream() << "Assertion failed: " #p0 " >= " #p1 " "

#define EXPECT_EQ(p0, p1) \
    if(!((p0) == (p1))) \
        g_failures_stream << std::endl << "Condition failed: " #p0 " == " #p1 " "

#define EXPECT_NE(p0, p1) \
    if(!((p0) != (p1))) \
        g_failures_stream << std::endl << "Condition failed: " #p0 " != " #p1 " "

int RUN_ALL_TESTS();

typedef void (TestFunction)();

struct TestRegistration
{
    TestRegistration(const std::string& test_case_name, const std::string& test_name, TestFunction* func);

    std::string test_case_name_;
    std::string test_name_;
    std::function<void()> func_;
};

extern std::ostringstream g_failures_stream;

