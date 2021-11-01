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

#pragma once

#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <functional>
#include <random>
#include <cmath>

#include <C2Component.h>
#include <gtest/gtest.h>
#include "mfx_c2_component.h"
#include "mfx_c2_utils.h"

// Collection of binary buffers hashes.
// The purpose is to check if component outputs differ between runs or the same.
class BinaryChunks
{
public:
    void PushBack(const uint8_t* data, size_t length)
    {
        std::basic_string<uint8_t> s(data, length);
        m_data.emplace_back(std::hash<std::basic_string<uint8_t>>()(s));
    }
    bool operator==(const BinaryChunks& other) const
    {
        return m_data == other.m_data;
    }
    bool operator!=(const BinaryChunks& other) const
    {
        return m_data != other.m_data;
    }
private:
    std::vector<size_t> m_data; // std::hash results
};

// Calculates CRC32 checksum
class CRC32Generator
{
public:
    CRC32Generator() {}

    void AddData(uint32_t width, uint32_t height, const uint8_t* data, size_t length);

    std::list<uint32_t> GetCrc32()
    {
        return m_crc32;
    }

private:
    uint32_t m_uCurWidth {};
    uint32_t m_uCurHeight {};
    std::list<uint32_t> m_crc32; // one crc32 for evety resolution change
};

// BinaryTester descendant simplifying BinaryWriter constructing for gtest tests
// It automatically gathered folders structure as ./<test_case_name>/<test_name>
class GTestBinaryWriter
{
public:
    GTestBinaryWriter(const std::string& name);
    // This overload helps shorten complicated file names construction.
    GTestBinaryWriter(const std::ostringstream& stream)
        : GTestBinaryWriter(stream.str()) { }

    void Write(const uint8_t* data, size_t length)
    {
        if (m_writer) {
            m_writer->Write(data, length);
        }
    }

    static void Enable(bool enable)
    {
        s_bEnabled = enable;
    }

private:
    std::unique_ptr<BinaryWriter> m_writer;

    static std::vector<std::string> GetTestFolders();

    static bool s_bEnabled;
};

class ComponentsCache : public testing::Environment
{
public:
    static ComponentsCache* GetInstance() { return s_cache; }

    std::shared_ptr<MfxC2Component> GetComponent(const char* name)
    {
        std::shared_ptr<MfxC2Component> result;
        auto it = m_components.find(name);
        if(it != m_components.end()) {
            result = it->second;
        }
        return result;
    }

    void PutComponent(const char* name, std::shared_ptr<MfxC2Component> component)
    {
        m_components.emplace(name, component);
    }

    void RemoveComponent(const char* name)
    {
        size_t count = m_components.erase(name);
        EXPECT_EQ(count, 1u);
    }

    void Clear()
    {
        for (auto& pair : m_components) {
            MfxC2Component* component = pair.second.get();
            c2_status_t res = ((C2Component*)component)->release();
            EXPECT_EQ(res, C2_OK);
        }
        m_components.clear();
    }

    void TearDown() override { Clear(); }

private:
    static ComponentsCache* Create()
    {
        ComponentsCache* cache = new ComponentsCache();
        ::testing::AddGlobalTestEnvironment(cache);
        return cache;
    }

private:
    static ComponentsCache* s_cache;

    std::map<std::string, std::shared_ptr<MfxC2Component>> m_components;
};

template<typename ComponentDesc>
std::shared_ptr<MfxC2Component> GetCachedComponent(const ComponentDesc& desc)
{
    std::shared_ptr<MfxC2Component> result =
        ComponentsCache::GetInstance()->GetComponent(desc.component_name);
    if (result == nullptr) {
        c2_status_t status = C2_OK;
        std::shared_ptr<MfxC2ParamReflector> reflector = std::make_shared<MfxC2ParamReflector>();
        MfxC2Component* mfx_component = MfxCreateC2Component(desc.component_name, desc.config, reflector, &status);

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
