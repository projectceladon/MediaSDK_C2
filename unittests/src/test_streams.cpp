/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "test_streams.h"
#include <map>
#include <gtest/gtest.h>

std::unique_ptr<StreamReader> StreamReader::Create(const std::vector<const StreamDescription*>& streams)
{
    std::unique_ptr<StreamReader> res;
    if (streams.size() == 1) {
        res = std::make_unique<SingleStreamReader>(streams[0]);
    } else if (streams.size() > 1) {
        res = std::make_unique<CombinedStreamReader>(streams);
    }
    return res;
}

bool CombinedStreamReader::Read(const Slicing& slicing, StreamDescription::Region* region, bool* header, size_t* start_code_len)
{
    bool res = false;
    for (; active_stream_index_ < readers_.size(); ++active_stream_index_) {
        res = readers_[active_stream_index_].Read(slicing, region, header, start_code_len);
        if (res) {
            region->offset += active_stream_offset_;
            break;
        }
        active_stream_offset_ += streams_[active_stream_index_]->data.size();
    }
    return res;
}

bool CombinedStreamReader::Seek(size_t pos)
{
    bool res = false;
    active_stream_offset_ = 0;

    for (size_t i = 0; i < streams_.size(); ++i) {
        if (pos < streams_[i]->data.size()) {
            res = readers_[i].Seek(pos);
            if (res) {
                if (i != active_stream_index_) {
                    if (active_stream_index_ < readers_.size()) {
                        readers_[active_stream_index_].Seek(0);
                    }
                    active_stream_index_ = i;
                }
            }
            break;
        }
        active_stream_offset_ += streams_[i]->data.size();
        pos -= streams_[i]->data.size();
    }

    return res;
}

bool CombinedStreamReader::EndOfStream()
{
    return (active_stream_index_ >= readers_.size()) ||
        ((active_stream_index_ == readers_.size() - 1) && readers_[active_stream_index_].EndOfStream());
}

std::vector<char> CombinedStreamReader::GetRegionContents(StreamDescription::Region region) const
{
    std::vector<char> res;

    for (const auto& reader : readers_) {
        if (region.size == 0) break;

        std::vector<char> chunk = reader.GetRegionContents(region);
        res.insert(res.end(), chunk.begin(), chunk.end());

        const StreamDescription* stream = streams_[&reader - &readers_.front()];
        // shift region for the next stream
        if (region.offset >= stream->data.size()) {
            region.offset -= stream->data.size();
        } else {
            region.offset = 0;
            region.size -= chunk.size();
        }
    }
    return res;
}

int32_t ReadBits(const std::vector<char>& data, StreamDescription::Region* region, uint8_t nBits /*1..32*/)
{
    int32_t res = 0;
    if (nBits > 32 || nBits == 0 ||
        (data.size() <= (region->offset + nBits / 8))) {
        return res;
    }
    if ((region->bit_offset + nBits) <= 8) { //read inside one byte
        char t_res = ((data[region->offset] & 0xFF) & (0xFF>>region->bit_offset))>>(8 - nBits - region->bit_offset);
        region->bit_offset += nBits;
        if (region->bit_offset == 8) {
            region->bit_offset = 0;
            region->offset++;
        }
        res = t_res;
    } else {
        uint32_t num_bits_for_read = nBits;
        if (region->bit_offset) {
            char t_res = ((data[region->offset] & 0xFF) & (0xFF>>region->bit_offset));
            region->offset++;
            num_bits_for_read -= 8 - region->bit_offset;
            res = t_res<<num_bits_for_read;
        }
        region->bit_offset = 0;
        while (num_bits_for_read) {
            uint32_t tmp = 0;
            if (num_bits_for_read >= 8) {
                num_bits_for_read -= 8;
                tmp = (data[region->offset] & 0xFF)<<num_bits_for_read;
                res = res | tmp;
                region->offset++;
            } else {
                tmp = ReadBits(data, region, num_bits_for_read);
                num_bits_for_read = 0;
                res = res | tmp;
            }
        }
    }
    return res;
}

void SkipEmulationBytes(std::vector<char>* data, StreamDescription::Region* region)
{
    // remove prevent code emulaton bytes
    if (data->size() >= region->offset + region->size) {
        const std::vector<char> delim = { 0, 0, 3 };
        std::vector<char>::iterator pos = data->begin() + region->offset;
        while (true) {
            std::vector<char>::iterator end = (data->begin() + region->offset + region->size);
            std::vector<char>::iterator found =
                std::search(pos, end, delim.begin(), delim.end());
            if (found != end) {
                data->erase(found + delim.size() - 1);
                region->size--;
            } else {
                return;
            }
            pos = found + delim.size() - 1;
        }
    }
}

bool ExtractAvcSequenceParameterSet(std::vector<char>&& bitstream, AvcSequenceParameterSet* sps)
{
    StreamDescription stream {};
    stream.data = std::move(bitstream); // do not init sps/pps regions, don't care of them

    SingleStreamReader reader(&stream);

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
                SkipEmulationBytes(&(stream.data), &region);
                region.offset += start_code_len + 1;
                sps->profile = ReadBits(stream.data, &region, 8);
                sps->constraints = ReadBits(stream.data, &region, 8);
                sps->level = ReadBits(stream.data, &region, 8);
                sps_found = true;
                break;
            }
        }
    }
    return sps_found;
}

bool ExtractHevcSequenceParameterSet(std::vector<char>&& bitstream, HevcSequenceParameterSet* sps)
{
    StreamDescription stream {};
    stream.data = std::move(bitstream); // do not init sps/pps regions, don't care of them
    SingleStreamReader reader(&stream);

    bool sps_found = false;

    StreamDescription::Region region {};
    bool header {};
    size_t start_code_len {};
    while (reader.Read(StreamReader::Slicing::NalUnit(), &region, &header, &start_code_len)) {
        if (region.size > start_code_len) {
            char header_byte = stream.data[region.offset + start_code_len]; // first byte start code
            uint8_t nal_unit_type = ((uint8_t)header_byte & 0x7E) >> 1;
            const uint8_t UNIT_TYPE_SPS = 33;
            if (nal_unit_type == UNIT_TYPE_SPS && region.size > start_code_len + 17) {
                SkipEmulationBytes(&(stream.data), &region);
                region.offset += start_code_len + 2;
                /*uint8_t video_parameter_set_id = */ReadBits(stream.data, &region, 4);
                /*uint8_t max_sub_layers_minus1 = */ReadBits(stream.data, &region, 3);
                /*bool temporal_id_nesting_flag = */ReadBits(stream.data, &region, 1);
                /*uint8_t profile_space = */ReadBits(stream.data, &region, 2);
                /*bool tier_flag = */ReadBits(stream.data, &region, 1);
                uint8_t profile_idc = ReadBits(stream.data, &region, 5);
                sps->profile = profile_idc;

                /*uint32_t profile_compatibility_flags = */ReadBits(stream.data, &region, 32);
                /*bool progressive_source_flag = */ReadBits(stream.data, &region, 1);
                /*bool interlaced_source_flag = */ReadBits(stream.data, &region, 1);
                /*bool non_packed_constraint_flag = */ReadBits(stream.data, &region, 1);
                /*bool frame_only_constraint_flag = */ReadBits(stream.data, &region, 1);

                /*uint32_t reserved_zero_43bits_0_19 =  */ReadBits(stream.data, &region, 20);
                /*uint32_t reserved_zero_43bits_20_42 = */ReadBits(stream.data, &region, 23);

                /*bool reserved_zero_bit = */ReadBits(stream.data, &region, 1);

                uint8_t level_idc = ReadBits(stream.data, &region, 8);
                sps->level = level_idc * 10 / 30;

                sps_found = true;
                break;
            }
        }
    }
    return sps_found;
}

bool TestAvcStreamProfileLevel(const C2ProfileLevelStruct& profile_level, std::vector<char>&& bitstream, std::string* message)
{
    struct SpsProfile {
        uint16_t sps_profile;
        uint16_t sps_constraints;
    };

    const std::map<C2Config::profile_t, SpsProfile> profile_to_sps = {
        { PROFILE_AVC_BASELINE, { 66, AvcSequenceParameterSet::Constraint::SET_1 } },
        { PROFILE_AVC_MAIN, { 77, 0 } },
        { PROFILE_AVC_EXTENDED, { 88, 0 } },
        { PROFILE_AVC_HIGH, { 100, 0 } },
    };

    struct TestLevel {
        const char* name;
        C2Config::level_t level;
        uint16_t sps_level;
    };

    const std::map<C2Config::level_t, uint16_t> level_to_sps = {
        { LEVEL_AVC_1, 1 },
        { LEVEL_AVC_1B, 9 },
        { LEVEL_AVC_1_1, 11 },
        { LEVEL_AVC_1_2, 12 },
        { LEVEL_AVC_1_3, 13 },
        { LEVEL_AVC_2, 20 },
        { LEVEL_AVC_2_1, 21 },
        { LEVEL_AVC_2_2, 22 },
        { LEVEL_AVC_3, 30 },
        { LEVEL_AVC_3_1, 31 },
        { LEVEL_AVC_3_2, 32 },
        { LEVEL_AVC_4, 40 },
        { LEVEL_AVC_4_1, 41 },
        { LEVEL_AVC_4_2, 42 },
        { LEVEL_AVC_5, 50 },
        { LEVEL_AVC_5_1, 51 },
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

bool TestHevcStreamProfileLevel(const C2ProfileLevelStruct& profile_level, std::vector<char>&& bitstream, std::string* message)
{
    struct SpsProfile {
        uint16_t sps_profile;
    };

    const std::map<C2Config::profile_t, SpsProfile> profile_to_sps = {
        { PROFILE_HEVC_MAIN, { 1 } },
        { PROFILE_HEVC_MAIN_10, { 2 } },
    };

    struct TestLevel {
        const char* name;
        C2Config::level_t level;
        uint16_t sps_level;
    };

    const std::map<C2Config::level_t, uint16_t> level_to_sps = {
        { LEVEL_HEVC_MAIN_1, 10 },
        { LEVEL_HEVC_MAIN_2, 20 },
        { LEVEL_HEVC_MAIN_2_1, 21 },
        { LEVEL_HEVC_MAIN_3, 30 },
        { LEVEL_HEVC_MAIN_3_1, 31 },
        { LEVEL_HEVC_MAIN_4, 40 },
        { LEVEL_HEVC_MAIN_4_1, 41 },
        { LEVEL_HEVC_MAIN_5, 50 },
        { LEVEL_HEVC_MAIN_5_1, 51 },
        { LEVEL_HEVC_MAIN_5_2, 52 },
        { LEVEL_HEVC_MAIN_6, 60 },
        { LEVEL_HEVC_MAIN_6_1, 61 },
        { LEVEL_HEVC_MAIN_6_2, 62 },
    };

    std::ostringstream oss;

    HevcSequenceParameterSet sps_actual;
    bool res = ExtractHevcSequenceParameterSet(std::move(bitstream), &sps_actual);
    if (!res) {
        oss << "sps is not found in bitstream\n";
    }

    if (res) {
        if (profile_to_sps.at(profile_level.profile).sps_profile != sps_actual.profile) {
            res = false;
            oss << "sps profile is " << sps_actual.profile << " instead of " << profile_to_sps.at(profile_level.profile).sps_profile << std::endl;
        }
        if (level_to_sps.at(profile_level.level) != sps_actual.level) {
            res = false;
            oss << "sps level is " << sps_actual.level << " instead of " << level_to_sps.at(profile_level.level) << std::endl;
        }
    }

    *message = oss.str();
    return res;
}
