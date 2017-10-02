/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "test_streams.h"
#include <map>
#include "gtest_emulation.h"
#include "mfx_legacy_defs.h"

using namespace android;

bool ExtractAvcSequenceParameterSet(std::vector<char>&& bitstream, AvcSequenceParameterSet* sps)
{
    StreamDescription stream {};
    stream.data = std::move(bitstream); // do not init sps/pps regions, don't care of them

    StreamReader reader(stream);

    bool sps_found = false;

    StreamDescription::Region region {};
    bool header {};
    size_t start_code_len {};
    while (reader.Read(StreamReader::Slicing::NalUnit(), &region, &header, &start_code_len)) {
        if (region.size > start_code_len) {
            char header_byte = stream.data[region.offset + start_code_len]; // first byte start code
            uint8_t nal_unit_type = (uint8_t)header_byte & 0x1F;
            const uint8_t UNIT_TYPE_SPS = 7;
            if (nal_unit_type == UNIT_TYPE_SPS && region.size > start_code_len + 3) {
                sps->profile = stream.data[region.offset + start_code_len + 1];
                sps->constraints = stream.data[region.offset + start_code_len + 2];
                sps->level = stream.data[region.offset + start_code_len + 3];
                sps_found = true;
                break;
            }
        }
    }
    return sps_found;
}

bool TestAvcStreamProfileLevel(const android::C2ProfileLevelStruct& profile_level, std::vector<char>&& bitstream, std::string* message)
{
    struct SpsProfile {
        uint16_t sps_profile;
        uint16_t sps_constraints;
    };

    const std::map<uint32_t, SpsProfile> profile_to_sps = {
        { LEGACY_VIDEO_AVCProfileBaseline, { 66, AvcSequenceParameterSet::Constraint::SET_1 } },
        { LEGACY_VIDEO_AVCProfileMain, { 77, 0 } },
        { LEGACY_VIDEO_AVCProfileExtended, { 88, 0 } },
        { LEGACY_VIDEO_AVCProfileHigh, { 100, 0 } },
    };

    struct TestLevel {
        const char* name;
        LEGACY_VIDEO_AVCLEVELTYPE level;
        uint16_t sps_level;
    };

    const std::map<uint32_t, uint16_t> level_to_sps = {
        { LEGACY_VIDEO_AVCLevel1, 1 },
        { LEGACY_VIDEO_AVCLevel1b, 9 },
        { LEGACY_VIDEO_AVCLevel11, 11 },
        { LEGACY_VIDEO_AVCLevel12, 12 },
        { LEGACY_VIDEO_AVCLevel13, 13 },
        { LEGACY_VIDEO_AVCLevel2, 20 },
        { LEGACY_VIDEO_AVCLevel21, 21 },
        { LEGACY_VIDEO_AVCLevel22, 22 },
        { LEGACY_VIDEO_AVCLevel3, 30 },
        { LEGACY_VIDEO_AVCLevel31, 31 },
        { LEGACY_VIDEO_AVCLevel32, 32 },
        { LEGACY_VIDEO_AVCLevel4, 40 },
        { LEGACY_VIDEO_AVCLevel41, 41 },
        { LEGACY_VIDEO_AVCLevel42, 42 },
        { LEGACY_VIDEO_AVCLevel5, 50 },
        { LEGACY_VIDEO_AVCLevel51, 51 },
    };

    std::ostringstream oss;

    AvcSequenceParameterSet sps_actual;
    bool res = ExtractAvcSequenceParameterSet(std::move(bitstream), &sps_actual);
    if (!res) {
        oss << "sps is not found in bitstream\n";
    }

    if (res) {
        if (profile_to_sps.at(profile_level.profile).sps_profile != sps_actual.profile) {
            res = false;
            oss << "sps profile is " << sps_actual.profile << " instead of " << profile_to_sps.at(profile_level.profile).sps_profile << std::endl;
        }
        if (profile_to_sps.at(profile_level.profile).sps_constraints != sps_actual.constraints) {
            res = false;
            oss << "sps constraints is " << sps_actual.constraints << " instead of " << profile_to_sps.at(profile_level.profile).sps_constraints << std::endl;
        }
        if (level_to_sps.at(profile_level.level) != sps_actual.level) {
            res = false;
            oss << "sps level is " << sps_actual.level << " instead of " << level_to_sps.at(profile_level.level) << std::endl;
        }
    }

    *message = oss.str();
    return res;
}
