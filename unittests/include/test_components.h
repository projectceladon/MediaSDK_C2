/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <vector>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <functional>
#include <random>
#include <cmath>

#include <C2Component.h>
#include <gtest/gtest.h>
#include "mfx_c2_component.h"

// Collection of binary buffers hashes.
// The purpose is to check if component outputs differ between runs or the same.
class BinaryChunks
{
public:
    void PushBack(const uint8_t* data, size_t length)
    {
        std::basic_string<uint8_t> s(data, length);
        data_.emplace_back(std::hash<std::basic_string<uint8_t>>()(s));
    }
    bool operator==(const BinaryChunks& other) const
    {
        return data_ == other.data_;
    }
    bool operator!=(const BinaryChunks& other) const
    {
        return data_ != other.data_;
    }
private:
    std::vector<size_t> data_; // std::hash results
};

// Calculates CRC32 checksum
class CRC32Generator
{
public:
    CRC32Generator() {}

    void AddData(uint32_t width, uint32_t height, const uint8_t* data, size_t length);

    std::list<uint32_t> GetCrc32()
    {
        return crc32_;
    }

private:
    uint32_t cur_width_ {};
    uint32_t cur_height_ {};
    std::list<uint32_t> crc32_; // one crc32 for evety resolution change
};

// Writes binary buffers to file.
class BinaryWriter
{
public:
    // File named <name> is created/overwritten in: ./<folders[0]>/.../<folders[N-1]>
    // Folders are created if missing.
    // So if a file with path ./Folder1/Folder2/File.txt is supposed to be written,
    // then it should be passed as: BinaryWriter( { "Folder1", "Folder2" }, "File.txt")
    BinaryWriter(const std::vector<std::string>& folders, const std::string& name);

public:
    void Write(const uint8_t* data, size_t length)
    {
        stream_.write((const char*)data, length);
    }

    static void Enable(bool enable)
    {
        enabled_ = enable;
    }
private:
    static bool enabled_;
    std::ofstream stream_;
};

// BinaryTester descendant simplifying BinaryWriter constructing for gtest tests
// It automatically gathered folders structure as ./<test_case_name>/<test_name>
class GTestBinaryWriter : public BinaryWriter
{
public:
    GTestBinaryWriter(const std::string& name);
    // This overload helps shorten complicated file names construction.
    GTestBinaryWriter(const std::ostringstream& stream)
        : GTestBinaryWriter(stream.str()) { }

private:
    static std::vector<std::string> GetTestFolders();
};

class ComponentsCache : public testing::Environment
{
public:
    static ComponentsCache* GetInstance() { return g_cache; }

    std::shared_ptr<MfxC2Component> GetComponent(const char* name)
    {
        std::shared_ptr<MfxC2Component> result;
        auto it = components_.find(name);
        if(it != components_.end()) {
            result = it->second;
        }
        return result;
    }

    void PutComponent(const char* name, std::shared_ptr<MfxC2Component> component)
    {
        components_.emplace(name, component);
    }

    void Clear() { components_.clear(); }

    void TearDown() override { Clear(); }

private:
    static ComponentsCache* Create()
    {
        ComponentsCache* cache = new ComponentsCache();
        ::testing::AddGlobalTestEnvironment(cache);
        return cache;
    }

private:
    static ComponentsCache* g_cache;

    std::map<std::string, std::shared_ptr<MfxC2Component>> components_;
};

template<typename ComponentDesc>
std::shared_ptr<MfxC2Component> GetCachedComponent(const ComponentDesc& desc)
{
    std::shared_ptr<MfxC2Component> result =
        ComponentsCache::GetInstance()->GetComponent(desc.component_name);
    if (result == nullptr) {
        c2_status_t status = C2_OK;
        MfxC2Component* mfx_component = MfxCreateC2Component(desc.component_name, desc.flags, &status);

        EXPECT_EQ(status, desc.creation_status);
        if(desc.creation_status == C2_OK) {
            EXPECT_NE(mfx_component, nullptr);
            result = std::shared_ptr<MfxC2Component>(mfx_component);
            ComponentsCache::GetInstance()->PutComponent(desc.component_name, result);
        }
    }
    return result;
}

typedef std::shared_ptr<C2Component> C2CompPtr;
typedef std::shared_ptr<C2ComponentInterface> C2CompIntfPtr;

template <typename Desc>
using ComponentTest =
    std::function<void(const Desc& desc, C2CompPtr comp, C2CompIntfPtr comp_intf)>;

inline void PrintAlphaNumeric(const char* text, ::std::ostream* os)
{
    for (const char* p = text; *p; ++p) {
        // Only alphanum and underscores allowed for test names.
        *os << (std::isalnum(*p) ? *p : '_');
    }
}

// Calls specified test std::function for every successfully created component.
template<typename ComponentDesc>
void CallComponentTest(const ComponentDesc& desc, ComponentTest<ComponentDesc> comp_test)
{
    do {
        SCOPED_TRACE(desc.component_name);

        std::shared_ptr<MfxC2Component> component = GetCachedComponent(desc);
        bool creation_expected = (desc.creation_status == C2_OK);
        bool creation_actual = (component != nullptr);

        EXPECT_EQ(creation_actual, creation_expected) << " for " << desc.component_name;
        if (nullptr == component) break;

        std::shared_ptr<C2Component> c2_component = component;
        std::shared_ptr<C2ComponentInterface> c2_component_intf = c2_component->intf();

        EXPECT_NE(c2_component_intf, nullptr);
        if (nullptr == c2_component_intf) break;

        comp_test(desc, c2_component, c2_component_intf);
    } while(false);
}

// Interface for filling/updating frame contents.
class FrameGenerator
{
public:
    virtual ~FrameGenerator() = default;
    // NV12 format is assumed
    virtual void Apply(uint32_t frame_index, uint8_t* data,
        uint32_t width, uint32_t stride, uint32_t height) = 0;
};

// Renders vertical bar code representing frame index, binary coded,
// frame divided vertically on 16 bars, every bar color represents one bit,
// less significant bit on the left.
class StripeGenerator : public FrameGenerator
{
public:
    // NV12 format is assumed
    void Apply(uint32_t frame_index, uint8_t* data,
        uint32_t width, uint32_t stride, uint32_t height) override;
};

// Gaussian white noise is applied over existing frame contents.
// Normal distribution with std variance == 5 is used.
class NoiseGenerator : public FrameGenerator
{
public:
    // NV12 format is assumed
    void Apply(uint32_t /*frame_index*/, uint8_t* data,
        uint32_t width, uint32_t stride, uint32_t height) override;
private:
    std::default_random_engine generator;
    std::normal_distribution<double> distribution { 0.5, 5.0 };
};
