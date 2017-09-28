/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "test_components.h"
#include "gtest_emulation.h"
#include "ipp.h"
#include <sstream>

void CRC32Generator::AddData(const uint8_t* data, size_t length)
{
    if (nullptr != data)
    {
        ippsCRC32_8u(data, length, &crc32_);
    }
}

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
        memcpy(row, top_row, stride); // copy top_row down the frame
        row += stride;
    }

    top_row = data + height * stride;
    RenderStripedRow(frame_index, width, false, top_row); // fill 1st chroma row
    row = top_row + stride;
    for(uint32_t i = 1; i < height / 2; ++i) {
        memcpy(row, top_row, stride); // copy top_row down the frame
        row += stride;
    }
}
