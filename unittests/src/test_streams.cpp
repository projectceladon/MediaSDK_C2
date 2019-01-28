/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

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

size_t CombinedStreamReader::GetPos() const
{
    size_t res = active_stream_offset_;
    if (active_stream_index_ < readers_.size()) {
        res += readers_[active_stream_index_].GetPos();
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

namespace HeaderParser
{
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

uint32_t Read_UE_V(const std::vector<char>& data, StreamDescription::Region* region) {
    int32_t leadingZeroBits = -1;
    for (int32_t b = 0; !b; leadingZeroBits++) { b = ReadBits(data, region, 1); }
    return (1<<leadingZeroBits) + ReadBits(data, region, leadingZeroBits) - 1;
}

int32_t Read_SE_V(const std::vector<char>& data, StreamDescription::Region* region) {
    uint32_t val = Read_UE_V(data, region);
    return (val & 0x01) ? (int32_t)((val + 1)>>1) : -(int32_t)((val + 1)>>1);
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

void AvcSequenceParameterSet::ParseVUI(const std::vector<char>& data, StreamDescription::Region* region)
{
    bool vui_aspect_ratio_info_present_flag = ReadBits(data, region, 1);
    if (vui_aspect_ratio_info_present_flag) {
        uint8_t vui_aspect_ratio_idc = ReadBits(data, region, 8);
        if (vui_aspect_ratio_idc == 255/*Extended_SAR*/) {
            /*uint16_t vui_sar_width = */ReadBits(data, region, 16);
            /*uint16_t vui_sar_height =  */ReadBits(data, region, 16);
        }
    }
    bool vui_overscan_info_present_flag = ReadBits(data, region, 1);
    if (vui_overscan_info_present_flag) {
        /*bool vui_overscan_appropriate_flag = */ReadBits(data, region, 1);
    }
    bool vui_video_signal_type_present_flag = ReadBits(data, region, 1);
    if (vui_video_signal_type_present_flag) {
        /*uint8_t vui_video_format = */ReadBits(data, region, 3);
        /*bool vui_video_full_range_flag = */ReadBits(data, region, 1);
        bool vui_colour_description_present_flag = ReadBits(data, region, 1);
        if (vui_colour_description_present_flag) {
            /*bool vui_colour_primaries = */ReadBits(data, region, 1);
            /*bool vui_transfer_characteristics = */ReadBits(data, region, 1);
            /*bool vui_matrix_coefficients = */ReadBits(data, region, 1);
        }
    }
    bool vui_chroma_loc_info_present_flag = ReadBits(data, region, 1);
    if (vui_chroma_loc_info_present_flag) {
        /*uint32_t vui_chroma_sample_loc_type_top_field = */Read_UE_V(data, region);
        /*uint32_t vui_chroma_sample_loc_type_bottom_field = */Read_UE_V(data, region);
    }
    bool vui_timing_info_present_flag = ReadBits(data, region, 1);
    if (vui_timing_info_present_flag) {
        uint32_t vui_num_units_in_tick = ReadBits(data, region, 32);
        uint32_t vui_time_scale = ReadBits(data, region, 32);
        /*bool vui_fixed_frame_rate_flag = */ReadBits(data, region, 1);
        frame_rate_ = ((float)vui_time_scale / 2) / vui_num_units_in_tick;
    }
}

bool AvcSequenceParameterSet::ExtractSequenceParameterSet(std::vector<char>&& bitstream)
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
            if (nal_unit_type == UNIT_TYPE_SPS) {
                SkipEmulationBytes(&(stream.data), &region);
                region.offset += start_code_len + 1;
                profile_ = ReadBits(stream.data, &region, 8);
                constraints_ = ReadBits(stream.data, &region, 8);
                level_ = ReadBits(stream.data, &region, 8);
                /*uint32_t seq_parameter_set_id = */Read_UE_V(stream.data, &region);
                if ( (profile_ == 100) || (profile_ == 110) || (profile_ == 122) ||
                    (profile_ == 244) || (profile_ == 44) || (profile_ == 83) ||
                    (profile_ == 86) || (profile_ == 118) || (profile_ == 128) ) {
                    uint32_t chroma_format_idc = Read_UE_V(stream.data, &region);
                    if (chroma_format_idc == 3) {
                        /*uint32_t separate_colour_plane_flag = */Read_UE_V(stream.data, &region);
                    }
                    /*uint32_t bit_depth_luma_minus8 = */Read_UE_V(stream.data, &region);
                    /*uint32_t bit_depth_chroma_minus8 = */Read_UE_V(stream.data, &region);
                    /*bool qpprime_y_zero_transform_bypass_flag = */ReadBits(stream.data, &region, 1);
                    /*bool seq_scaling_matrix_present_flag = */ReadBits(stream.data, &region, 1);
                }
                /*uint32_t log2_max_frame_num_minus4 = */Read_UE_V(stream.data, &region);
                uint32_t pic_order_cnt_type = Read_UE_V(stream.data, &region);
                if (pic_order_cnt_type == 0) {
                    /*uint32_t log2_max_pic_order_cnt_lsb_minus4_ = */Read_UE_V(stream.data, &region);
                } else if (pic_order_cnt_type == 1) {
                    /*bool delta_pic_order_always_zero_flag = */ReadBits(stream.data, &region, 1);
                    /*int32_t offset_for_non_ref_pic = */Read_SE_V(stream.data, &region);
                    /*int32_t offset_for_top_to_bottom_field = */Read_SE_V(stream.data, &region);
                    uint32_t num_ref_frames_in_pic_order_cnt_cycle = Read_UE_V(stream.data, &region);
                    for( uint32_t i = 0; i < num_ref_frames_in_pic_order_cnt_cycle; i++ ) {
                        /*int32_t offset_for_ref_frame = */Read_SE_V(stream.data, &region);
                    }
                }
                /*uint32_t max_num_ref_frames = */Read_UE_V(stream.data, &region);
                /*bool gaps_in_frame_num_value_allowed_flag = */ReadBits(stream.data, &region, 1);
                /*uint32_t pic_width_in_mbs_minus1 = */Read_UE_V(stream.data, &region);
                /*uint32_t pic_height_in_map_units_minus1 = */Read_UE_V(stream.data, &region);
                bool frame_mbs_only_flag = ReadBits(stream.data, &region, 1);
                if (!frame_mbs_only_flag){
                    /*bool mb_adaptive_frame_field_flag = */ReadBits(stream.data, &region, 1);
                }
                /*bool direct_8x8_inference_flag = */ReadBits(stream.data, &region, 1);
                bool frame_cropping_flag = ReadBits(stream.data, &region, 1);
                if (frame_cropping_flag) {
                    /*uint32_t frame_crop_left_offset = */Read_UE_V(stream.data, &region);
                    /*uint32_t frame_crop_right_offset = */Read_UE_V(stream.data, &region);
                    /*uint32_t frame_crop_top_offset = */Read_UE_V(stream.data, &region);
                    /*uint32_t frame_crop_bottom_offset = */Read_UE_V(stream.data, &region);
                }
                bool vui_parameters_present_flag = ReadBits(stream.data, &region, 1);
                if (vui_parameters_present_flag){
                    ParseVUI(stream.data, &region);
                }
                sps_found = true;
                break;
            }
        }
    }
    return sps_found;
}

void HevcSequenceParameterSet::ParsePTL(const std::vector<char>& data, StreamDescription::Region* region)
{
    /*uint8_t profile_space = */ReadBits(data, region, 2);
    /*bool tier_flag = */ReadBits(data, region, 1);
    profile_ = ReadBits(data, region, 5);
    uint32_t profile_compatibility_flags = ReadBits(data, region, 32);
    /*bool progressive_source_flag = */ReadBits(data, region, 1);
    /*bool interlaced_source_flag = */ReadBits(data, region, 1);
    /*bool non_packed_constraint_flag = */ReadBits(data, region, 1);
    /*bool frame_only_constraint_flag = */ReadBits(data, region, 1);
    if (ProfileMatches(profile_, Profiles::HEVC_REXT, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_REXT_HT, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_MAIN_MV, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_MAIN_SC, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_MAIN_3D, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_SCC, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_REXT_SC, profile_compatibility_flags)) {
            /*bool max_12bit_constraint_flag = */ReadBits(data, region, 1);
            /*bool max_10bit_constraint_flag = */ReadBits(data, region, 1);
            /*bool max_8bit_constraint_flag = */ReadBits(data, region, 1);
            /*bool max_422chroma_constraint_flag = */ReadBits(data, region, 1);
            /*bool max_420chroma_constraint_flag = */ReadBits(data, region, 1);
            /*bool max_monochrome_constraint_flag = */ReadBits(data, region, 1);
            /*bool intra_constraint_flag = */ReadBits(data, region, 1);
            /*bool one_picture_only_constraint_flag = */ReadBits(data, region, 1);
            /*bool lower_bit_rate_constraint_flag = */ReadBits(data, region, 1);
            if (ProfileMatches(profile_, Profiles::HEVC_REXT_HT, profile_compatibility_flags) ||
                ProfileMatches(profile_, Profiles::HEVC_SCC, profile_compatibility_flags) ||
                ProfileMatches(profile_, Profiles::HEVC_REXT_SC, profile_compatibility_flags)) {
                /*bool max_14bit_constraint_flag = */ReadBits(data, region, 1);
                /*uint16_t reserved_zero_33bits_0_9 = */ReadBits(data, region, 10);
                /*uint32_t reserved_zero_33bits_10_32 = */ReadBits(data, region, 23);
            }
            else
            {
                /*uint16_t reserved_zero_34bits_0_10 = */ReadBits(data, region, 11);
                /*uint32_t reserved_zero_34bits_11_33 = */ReadBits(data, region, 23);
            }
    } else {
            /*uint32_t reserved_zero_43bits_0_19 =  */ReadBits(data, region, 20);
            /*uint32_t reserved_zero_43bits_20_42 = */ReadBits(data, region, 23);
    }
    if (ProfileMatches(profile_, Profiles::HEVC_MAIN, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_MAIN_10, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_MAIN_SP, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_REXT, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_REXT_HT, profile_compatibility_flags) ||
        ProfileMatches(profile_, Profiles::HEVC_SCC, profile_compatibility_flags)) {
            /*bool inbld_flag = */ReadBits(data, region, 1);
    } else {
            /*bool reserved_zero_bit = */ReadBits(data, region, 1);
    }
    uint8_t level_idc = ReadBits(data, region, 8);
    level_ = level_idc * 10 / 30;
    std::vector<bool> sub_profile_present_flag;
    std::vector<bool> sub_level_present_flag;
    for (uint8_t i = 0; i < max_sub_layers_minus1_; i++) {
        sub_profile_present_flag.push_back((bool)ReadBits(data, region, 1));
        sub_level_present_flag.push_back((bool)ReadBits(data, region, 1));
    }
    if (max_sub_layers_minus1_) {
        ReadBits(data, region, ((8 - max_sub_layers_minus1_) * 2));
    }
    for (uint8_t i = 0; i < max_sub_layers_minus1_; i++) {
        if (sub_profile_present_flag[i]) {
            ReadBits(data, region, 96);//read 12 bytes
        }
        if (sub_level_present_flag[i]) {
            ReadBits(data, region, 8);
        }
    }

}

void HevcSequenceParameterSet::ParseSLD(const std::vector<char>& data, StreamDescription::Region* region)
{
    for (uint8_t i = 0; i < 4; i++) {
        for (uint8_t j = 0; j < 6; j += (i == 3) ? 3 : 1) {
            bool scaling_list_pred_mode_flag = ReadBits(data, region, 1);
            if (!scaling_list_pred_mode_flag) {
                Read_UE_V(data, region);
            } else {
                Read_SE_V(data, region);
            }
        }
    }

}

void HevcSequenceParameterSet::ParseSTRPS(const std::vector<char>& data, StreamDescription::Region* region)
{
    uint32_t num_short_term_ref_pic_sets = Read_UE_V(data, region);
    for (uint32_t i = 0; i < num_short_term_ref_pic_sets; i++) {
        bool inter_ref_pic_set_prediction_flag = 0;
        if (i) {
            ReadBits(data, region, 1);
        }
        if (inter_ref_pic_set_prediction_flag) {
            ReadBits(data, region, 1);
            Read_UE_V(data, region);
            Read_UE_V(data, region);
            //N is NumDeltaPocs of reference STRPS (i -1) - 0 in case of SPS
        /*for (uint8_t j = 0; j < N; j++) {
                bool used_by_curr_pic_flag = ReadBits(stream.data, &region, 1);
                bool use_delta_flag = ReadBits(stream.data, &region, 1);
            }*/
        } else {
            uint16_t num_negative_pics = Read_UE_V(data, region);
            uint16_t n = num_negative_pics + Read_UE_V(data, region);
            for (uint16_t j = 0; j < n; j++) {
                /*uint16_t delta_poc_s0_minus1 = */Read_UE_V(data, region);
                /*uint16_t used_by_curr_pic_s0_flag = */ReadBits(data, region, 1);
            }
        }
    }
}

void HevcSequenceParameterSet::ParseLTRPS(const std::vector<char>& data, StreamDescription::Region* region)
{
    uint32_t num_long_term_ref_pics = Read_UE_V(data, region);
    if (num_long_term_ref_pics) {
        for (uint32_t i = 0; i < num_long_term_ref_pics; i++)
        {
            ReadBits(data, region, log2_max_pic_order_cnt_lsb_minus4_ + 4);
            ReadBits(data, region, 1);
        }
    }
}

void HevcSequenceParameterSet::ParseVUI(const std::vector<char>& data, StreamDescription::Region* region)
{
    bool vui_aspect_ratio_info_present_flag = ReadBits(data, region, 1);
    if (vui_aspect_ratio_info_present_flag) {
        uint8_t vui_aspect_ratio_idc = ReadBits(data, region, 8);
        if (vui_aspect_ratio_idc == 255) {
            /*uint16_t vui_sar_width = */ReadBits(data, region, 16);
            /*uint16_t vui_sar_height = */ReadBits(data, region, 16);
        }
    }
    bool vui_overscan_info_present_flag = ReadBits(data, region, 1);
    if (vui_overscan_info_present_flag) {
        /*bool vui_overscan_appropriate_flag = */ReadBits(data, region, 1);
    }
    bool vui_video_signal_type_present_flag = ReadBits(data, region, 1);
    if (vui_video_signal_type_present_flag) {
        /*uint8_t vui_video_format = */ReadBits(data, region, 3);
        /*bool vui_video_full_range_flag = */ReadBits(data, region, 1);
        bool vui_colour_description_present_flag = ReadBits(data, region, 1);
        if (vui_colour_description_present_flag) {
            /*uint8_t vui_colour_primaries = */ReadBits(data, region, 8);
            /*uint8_t vui_transfer_characteristics = */ReadBits(data, region, 8);
            /*uint8_t vui_matrix_coeffs = */ReadBits(data, region, 8);
        }
    }
    bool vui_chroma_loc_info_present_flag = ReadBits(data, region, 1);
    if (vui_chroma_loc_info_present_flag) {
        /*uint32_t vui_chroma_sample_loc_type_top_field = */Read_UE_V(data, region);
        /*uint32_t vui_chroma_sample_loc_type_bottom_field = */Read_UE_V(data, region);
    }
    /*bool vui_neutral_chroma_indication_flag = */ReadBits(data, region, 1);
    /*bool vui_field_seq_flag = */ReadBits(data, region, 1);
    /*bool vui_frame_field_info_present_flag = */ReadBits(data, region, 1);
    bool vui_default_display_window_flag = ReadBits(data, region, 1);
    if (vui_default_display_window_flag) {
        /*uint32_t vui_def_disp_win_left_offset = */Read_UE_V(data, region);
        /*uint32_t vui_def_disp_win_right_offset = */Read_UE_V(data, region);
        /*uint32_t vui_def_disp_win_top_offset = */Read_UE_V(data, region);
        /*uint32_t vui_def_disp_win_bottom_offset = */Read_UE_V(data, region);
    }
    bool vui_timing_info_present_flag = ReadBits(data, region, 1);
    if (vui_timing_info_present_flag) {
        uint32_t vui_num_units_in_tick = ReadBits(data, region, 32);
        uint32_t vui_time_scale = ReadBits(data, region, 32);
        bool vui_poc_proportional_to_timing_flag = ReadBits(data, region, 1);
        if (vui_poc_proportional_to_timing_flag) {
            /*uint32_t vui_num_ticks_poc_diff_one_minus1 = */Read_UE_V(data, region);
        }
        frame_rate_ = (float)vui_time_scale / vui_num_units_in_tick;
        bool vui_hrd_parameters_present_flag = ReadBits(data, region, 1);
        if (vui_hrd_parameters_present_flag) {
            //HRD parsing
        }
    }

}

bool HevcSequenceParameterSet::ExtractSequenceParameterSet(std::vector<char>&& bitstream)
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
            if (nal_unit_type == UNIT_TYPE_SPS) {
                SkipEmulationBytes(&(stream.data), &region);
                region.offset += start_code_len + 2;
                /*uint8_t video_parameter_set_id = */ReadBits(stream.data, &region, 4);
                max_sub_layers_minus1_ = ReadBits(stream.data, &region, 3);
                /*bool temporal_id_nesting_flag = */ReadBits(stream.data, &region, 1);
                ParsePTL(stream.data, &region);
                /*uint32_t seq_parameter_set_id = */Read_UE_V(stream.data, &region);
                uint32_t chroma_format_idc = Read_UE_V(stream.data, &region);
                if (chroma_format_idc == 3/*CHROMA_444*/) {
                    /*bool temporal_id_nesting_flag = */ReadBits(stream.data, &region, 1);
                }
                /*uint32_t pic_width_in_luma_samples = */Read_UE_V(stream.data, &region);
                /*uint32_t pic_height_in_luma_samples = */Read_UE_V(stream.data, &region);
                bool conformance_window_flag = ReadBits(stream.data, &region, 1);
                if (conformance_window_flag) {
                    /*uint32_t conf_win_left_offset = */Read_UE_V(stream.data, &region);
                    /*uint32_t conf_win_right_offset = */Read_UE_V(stream.data, &region);
                    /*uint32_t conf_win_top_offset = */Read_UE_V(stream.data, &region);
                    /*uint32_t conf_win_bottom_offset = */Read_UE_V(stream.data, &region);
                }
                /*uint32_t bit_depth_luma_minus8 = */Read_UE_V(stream.data, &region);
                /*uint32_t bit_depth_chroma_minus8 = */Read_UE_V(stream.data, &region);
                log2_max_pic_order_cnt_lsb_minus4_ = Read_UE_V(stream.data, &region);
                bool sub_layer_ordering_info_present_flag = ReadBits(stream.data, &region, 1);
                if (max_sub_layers_minus1_ >= 0 && max_sub_layers_minus1_ <= 7) {
                    Read_UE_V(stream.data, &region);
                    Read_UE_V(stream.data, &region);
                    Read_UE_V(stream.data, &region);
                }
                if (sub_layer_ordering_info_present_flag) {
                    for (uint8_t i = 0; i < max_sub_layers_minus1_; i++) {
                        Read_UE_V(stream.data, &region);
                        Read_UE_V(stream.data, &region);
                        Read_UE_V(stream.data, &region);
                    }
                }
                /*uint32_t log2_min_luma_coding_block_size_minus3 = */Read_UE_V(stream.data, &region);
                /*uint32_t log2_diff_max_min_luma_coding_block_size = */Read_UE_V(stream.data, &region);
                /*uint32_t log2_min_luma_transform_block_size_minus2 = */Read_UE_V(stream.data, &region);
                /*uint32_t log2_diff_max_min_luma_transform_block_size = */Read_UE_V(stream.data, &region);
                /*uint32_t max_transform_hierarchy_depth_inter = */Read_UE_V(stream.data, &region);
                /*uint32_t max_transform_hierarchy_depth_intra = */Read_UE_V(stream.data, &region);
                bool scaling_list_enabled_flag = ReadBits(stream.data, &region, 1);
                if (scaling_list_enabled_flag) {
                    bool scaling_list_data_present_flag = ReadBits(stream.data, &region, 1);
                    if (scaling_list_data_present_flag) {
                        ParseSLD(stream.data, &region);
                    }
                }
                /*bool amp_enabled_flag = */ReadBits(stream.data, &region, 1);
                /*bool sample_adaptive_offset_enabled_flag = */ReadBits(stream.data, &region, 1);
                bool pcm_enabled_flag = ReadBits(stream.data, &region, 1);
                if (pcm_enabled_flag) {
                    /*uint8_t pcm_sample_bit_depth_luma_minus1 = */ReadBits(stream.data, &region, 4);
                    /*uint8_t pcm_sample_bit_depth_chroma_minus1 = */ReadBits(stream.data, &region, 4);
                    /*uint32_t log2_min_pcm_luma_coding_block_size_minus3 = */Read_UE_V(stream.data, &region);
                    /*uint32_t log2_diff_max_min_pcm_luma_coding_block_size = */Read_UE_V(stream.data, &region);
                    /*bool pcm_loop_filter_disabled_flag = */ReadBits(stream.data, &region, 1);
                }
                ParseSTRPS(stream.data, &region);
                bool long_term_ref_pics_present_flag = ReadBits(stream.data, &region, 1);
                if (long_term_ref_pics_present_flag) {
                    ParseLTRPS(stream.data, &region);
                }
                /*bool temporal_mvp_enabled_flag = */ReadBits(stream.data, &region, 1);
                /*bool strong_intra_smoothing_enabled_flag = */ReadBits(stream.data, &region, 1);

                bool vui_parameters_present_flag = ReadBits(stream.data, &region, 1);
                if (vui_parameters_present_flag) {
                    ParseVUI(stream.data, &region);
                }
                sps_found = true;
                break;
            }
        }
    }
    return sps_found;
}
}//namespace HeaderParser

bool TestAvcStreamProfileLevel(const C2ProfileLevelStruct& profile_level, std::vector<char>&& bitstream, std::string* message)
{
    struct SpsProfile {
        uint16_t sps_profile;
        uint16_t sps_constraints;
    };

    const std::map<C2Config::profile_t, SpsProfile> profile_to_sps = {
        { PROFILE_AVC_BASELINE, { 66, HeaderParser::AvcSequenceParameterSet::Constraint::SET_1 } },
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

    HeaderParser::AvcSequenceParameterSet sps_actual;
    bool res = sps_actual.ExtractSequenceParameterSet(std::move(bitstream));
    if (!res) {
        oss << "sps is not found in bitstream\n";
    }

    if (res) {
        if (profile_to_sps.at(profile_level.profile).sps_profile != sps_actual.profile_) {
            res = false;
            oss << "sps profile is " << sps_actual.profile_ << " instead of " << profile_to_sps.at(profile_level.profile).sps_profile << std::endl;
        }
        if (profile_to_sps.at(profile_level.profile).sps_constraints != sps_actual.constraints_) {
            res = false;
            oss << "sps constraints is " << sps_actual.constraints_ << " instead of " << profile_to_sps.at(profile_level.profile).sps_constraints << std::endl;
        }
        if (level_to_sps.at(profile_level.level) != sps_actual.level_) {
            res = false;
            oss << "sps level is " << sps_actual.level_ << " instead of " << level_to_sps.at(profile_level.level) << std::endl;
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

    HeaderParser::HevcSequenceParameterSet sps_actual;
    bool res = sps_actual.ExtractSequenceParameterSet(std::move(bitstream));
    if (!res) {
        oss << "sps is not found in bitstream\n";
    }

    if (res) {
        if (profile_to_sps.at(profile_level.profile).sps_profile != sps_actual.profile_) {
            res = false;
            oss << "sps profile is " << sps_actual.profile_ << " instead of " << profile_to_sps.at(profile_level.profile).sps_profile << std::endl;
        }
        if (level_to_sps.at(profile_level.level) != sps_actual.level_) {
            res = false;
            oss << "sps level is " << sps_actual.level_ << " instead of " << level_to_sps.at(profile_level.level) << std::endl;
        }
    }

    *message = oss.str();
    return res;
}
