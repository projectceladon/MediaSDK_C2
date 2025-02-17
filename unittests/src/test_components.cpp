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

#include "test_components.h"
#include <sstream>
#include <zlib.h>

void CRC32Generator::AddData(uint32_t width, uint32_t height, const uint8_t* data, size_t length)
{
    if (nullptr != data)
    {
        if (m_uCurWidth != width || m_uCurHeight != height) {
            m_crc32.push_back(0);
            m_uCurWidth = width;
            m_uCurHeight = height;
        }
        m_crc32.back() = crc32(m_crc32.back(), data, length);
    }
}

GTestBinaryWriter::GTestBinaryWriter(const std::string& name)
{
    if (s_bEnabled) {
        m_writer = std::make_unique<BinaryWriter>(".", GetTestFolders(), name);
    }
}

std::vector<std::string> GTestBinaryWriter::GetTestFolders()
{
    const ::testing::TestInfo* const test_info =
        ::testing::UnitTest::GetInstance()->current_test_info();
    std::vector<std::string> res;
    for (const std::string& s : {test_info->test_case_name(), test_info->name()} ) {
        std::stringstream ss(s);
        std::string part;
        while (std::getline(ss, part, '/')) {
            if (!part.empty()) {
                res.push_back(part);
            }
        }
    }
    return res;
}

bool GTestBinaryWriter::s_bEnabled = false;

ComponentsCache* ComponentsCache::s_cache = ComponentsCache::Create();

void NoiseGenerator::Apply(uint32_t /*frame_index*/, uint8_t* data,
    uint32_t width, uint32_t stride, uint32_t height)
{
    // apply some noise
    for(uint32_t j = 0; j < height * 3 / 2; ++j) {
        for(uint32_t i = 0; i < width; ++i) {
            double normal_value = distribution(generator);
            data[i] = ClampCast<uint8_t>(data[i] + static_cast<int>(floor(normal_value)));
        }
        data += stride;
    }
}

const uint32_t STRIPE_COUNT = 16;

struct YuvColor
{
    uint8_t Y;
    uint8_t U;
    uint8_t V;
};

const YuvColor FRAME_STRIPE_COLORS[2] =
{
    { 20, 230, 20 }, // dark-blue
    { 150, 60, 230 } // bright-red
};

// Renders one row of striped image, in NV12 format.
// Stripes are binary figuring frame_index.
static void RenderStripedRow(uint32_t frame_index, uint32_t frame_width, bool luma, uint8_t* row)
{
    int x = 0;
    for(uint32_t s = 0; s < STRIPE_COUNT; ++s) {
        int stripe_right_edge = (s + 1) * (frame_width / 2) / STRIPE_COUNT; // in 2x2 blocks
        const YuvColor& color = FRAME_STRIPE_COLORS[frame_index & 1/*lower bit*/];

        for(; x < stripe_right_edge; ++x) {
            if(luma) {
                row[2 * x] = color.Y;
                row[2 * x + 1] = color.Y;
            } else {
                row[2 * x] = color.U;
                row[2 * x + 1] = color.V;
            }
        }

        frame_index >>= 1; // next bit
    }
}

void StripeGenerator::Apply(uint32_t frame_index, uint8_t* data,
    uint32_t width, uint32_t stride, uint32_t height)
{
    uint8_t* top_row = data;
    RenderStripedRow(frame_index, width, true, top_row); // fill 1st luma row
    uint8_t* row = top_row + stride;
    for(uint32_t i = 1; i < height; ++i) {
        std::copy(top_row, top_row + stride, row); // copy top_row down the frame
        row += stride;
    }

    top_row = data + height * stride;
    RenderStripedRow(frame_index, width, false, top_row); // fill 1st chroma row
    row = top_row + stride;
    for(uint32_t i = 1; i < height / 2; ++i) {
        std::copy(top_row, top_row + stride, row); // copy top_row down the frame
        row += stride;
    }
}
