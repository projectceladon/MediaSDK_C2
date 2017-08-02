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
    static testing::TestRegistration g_##test_case_name##test_name (#test_case_name, #test_name, &test_case_name##test_name); \
    void test_case_name##test_name()

#define ASSERT_NE(p0, p1) \
    if(!((p0) != (p1))) \
        throw std::ostringstream() << testing::CutPath(__FILE__) << ":" << __LINE__  << " Assertion failed: " #p0 " != " #p1 " "

#define ASSERT_EQ(p0, p1) \
    if(!((p0) == (p1))) \
        throw std::ostringstream() << testing::CutPath(__FILE__) << ":" << __LINE__  << " Assertion failed: " #p0 " == " #p1 " "

#define ASSERT_GE(p0, p1) \
    if(!((p0) >= (p1))) \
        throw std::ostringstream() << testing::CutPath(__FILE__) << ":" << __LINE__  << " Assertion failed: " #p0 " >= " #p1 " "

#define EXPECT_EQ(p0, p1) \
    if(!((p0) == (p1))) \
        testing::g_failures_stream << std::endl << testing::CutPath(__FILE__) << ":" << __LINE__  << " Condition failed: " #p0 " == " #p1 " "

#define EXPECT_NE(p0, p1) \
    if(!((p0) != (p1))) \
        testing::g_failures_stream << std::endl << testing::CutPath(__FILE__) << ":" << __LINE__  << " Condition failed: " #p0 " != " #p1 " "

int RUN_ALL_TESTS();

namespace testing {

class TestInfo
{
public:
    // Returns the test case name and the test name, respectively.
    //
    // Do NOT delete or free the return value - it's managed by the
    // TestInfo class.
    const char* test_case_name() const
    {
        return test_case_name_.c_str();
    }

    const char* name() const
    {
        return name_.c_str();
    }

public:
    std::string test_case_name_;

    std::string name_;
};

class UnitTest
{
public:
    static UnitTest* GetInstance()
    {
        static UnitTest s_instance;
        return &s_instance;
    }

    TestInfo* current_test_info()
    {
        return &current_test_info_;
    }

public:
    TestInfo current_test_info_;
};

void InitGoogleTest(int* argc, char** argv);

const char* CutPath(const char* path);

typedef void (TestFunction)();

struct TestRegistration
{
    TestRegistration(const std::string& test_case_name, const std::string& test_name, TestFunction* func);

    std::string test_case_name_;
    std::string test_name_;
    std::function<void()> func_;
};

extern std::ostringstream g_failures_stream;

} // namespace testing
