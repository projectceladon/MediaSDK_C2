/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <vector>
#include "mfx_c2_params.h"

struct StreamDescription
{
    struct Region
    {
        size_t offset;
        size_t size;
        bool Intersects(const Region& other) const {
            return offset < (other.offset + other.size) &&
                other.offset < (offset + size);
        }
    };
    Region sps;
    Region pps;

    uint32_t crc32_nv12; // Checksum of the video decoded to nv12 format,
    // obtained with command line: ./mfx_player64 -i {bitstream}.264 -crc -hw -o:nv12
    // It does not match default crc32 checksums which are actually for I420.

    std::vector<char> data;
};

// Reads chunks from binary stream,
// the chunks splitting depends on StreamReader::Type
class StreamReader
{
public:
    // Sets how chunks are split
    class Slicing
    {
    public:
        enum class Type {
            NalUnit, // by NAL units
            Frame,
            Fixed
        };

    public:
        Slicing(size_t size) : type_(Type::Fixed), size_(size) {}

        static Slicing NalUnit()
        {
            Slicing res(0);
            res.type_ = Type::NalUnit;
            return res;
        };

        static Slicing Frame()
        {
            Slicing res(0);
            res.type_ = Type::Frame;
            return res;
        };

        Type GetType() const { return type_; }

        size_t GetSize() const { return size_; }

    private:
        Type type_;

        size_t size_;
    };

public:
    StreamReader(const StreamDescription& stream):
        stream_(stream),
        pos_(stream.data.begin())
        {}
    // reads next stream chunk specified in slicing
    bool Read(const Slicing& slicing, StreamDescription::Region* region, bool* header, size_t* start_code_len = nullptr);

    bool ContainsHeader(const StreamDescription::Region& region);

    bool ContainsSlice(const StreamDescription::Region& region, size_t start_code_len);

    bool Seek(size_t pos);

private:
    const StreamDescription& stream_;
    std::vector<char>::const_iterator pos_;
};

inline bool StreamReader::Read(const Slicing& slicing, StreamDescription::Region* region, bool* header, size_t* start_code_len)
{
    bool res = true;
    if(pos_ < stream_.data.end()) {
        switch(slicing.GetType()) {
            case Slicing::Type::NalUnit: {
                const std::vector<std::vector<char>> delims =
                    { { 0, 0, 0, 1 }, { 0, 0, 1 } };

                std::vector<char>::const_iterator find_from = pos_;
                for (const auto& delim : delims) {
                    std::vector<char>::const_iterator found =
                        std::search(pos_, stream_.data.end(), delim.begin(), delim.end());
                    if (found == pos_) { // this is current beginning of current nal unit, skip it
                        find_from = pos_ + delim.size();
                        break;
                    }
                }

                std::vector<char>::const_iterator nearest_delim = stream_.data.end();
                for (const auto& delim : delims) {
                    std::vector<char>::const_iterator found =
                        std::search(find_from, stream_.data.end(), delim.begin(), delim.end());
                    if (found < nearest_delim) {
                        nearest_delim = found;
                    }
                }
                region->offset = pos_ - stream_.data.begin();
                region->size = nearest_delim - pos_;
                *header = ContainsHeader(*region);
                if (nullptr != start_code_len) {
                    *start_code_len = find_from - pos_;
                }
                pos_ = nearest_delim;
                break;
            }
            case Slicing::Type::Frame: {
                StreamDescription::Region tmp_region;
                bool tmp_header;
                size_t tmp_start_code_len = 0;

                res = Read(Slicing::NalUnit(), &tmp_region, &tmp_header, &tmp_start_code_len);
                if (nullptr != start_code_len) {
                    *start_code_len = tmp_start_code_len;
                }
                if (res) {
                    region->offset = tmp_region.offset;
                    region->size = tmp_region.size;
                    *header = tmp_header;

                    bool tmp_res = true;
                    while (!ContainsSlice(tmp_region, tmp_start_code_len)) {
                        tmp_res = Read(Slicing::NalUnit(), &tmp_region, &tmp_header, &tmp_start_code_len);
                        if (tmp_res) {
                            region->size += tmp_region.size;
                            *header = (*header || tmp_header);
                        } else {
                            break;
                        }
                    }
                    // Check if it is the last slice and there are NAL units onwards
                    if (tmp_res) {
                        std::vector<char>::const_iterator tmp_pos = pos_;
                        size_t tail = 0;
                        while (true) {
                            tmp_res = Read(Slicing::NalUnit(), &tmp_region, &tmp_header, &tmp_start_code_len);
                            if (tmp_res) {
                                tail += tmp_region.size;
                            } else {
                                // Current slice was the last slice, so we need to add rest NAL units
                                break;
                            }
                            if (ContainsSlice(tmp_region, tmp_start_code_len)) {
                                // The slice was not the last one - we found next slice
                                // The data between the slices will be copied in the next call
                                tail = 0;
                                pos_ = tmp_pos;
                                break;
                            }
                        }
                        region->size += tail;
                    }
                }
                break;
            }
            case Slicing::Type::Fixed: {
                region->offset = pos_ - stream_.data.begin();
                region->size = std::min<size_t>(stream_.data.end() - pos_, slicing.GetSize());
                *header = ContainsHeader(*region);
                pos_ += region->size;
                break;
            }
            default:
                res = false;
                break;
        }
    } else {
        res = false;
    }
    return res;
}

inline bool StreamReader::ContainsHeader(const StreamDescription::Region& region)
{
    return stream_.sps.Intersects(region) || stream_.pps.Intersects(region);
}

inline bool StreamReader::ContainsSlice(const StreamDescription::Region& region, size_t start_code_len)
{
    bool is_slice = false;
    if (region.size >= start_code_len + 1) {
        char header_byte = stream_.data[region.offset + start_code_len]; // first byte after start code
        uint8_t nal_unit_type = (uint8_t)header_byte & 0x1F;
        if(nal_unit_type == 1 || nal_unit_type == 5) { // slice types
            is_slice = true;
        }
    }
    return is_slice;
}

inline bool StreamReader::Seek(size_t pos)
{
    bool res = true;
    if(pos <= stream_.data.size()) {
        pos_ = stream_.data.begin() + pos;
    } else {
        res = false;
    }
    return res;
}

struct AvcSequenceParameterSet
{
    uint16_t profile;
    enum Constraint : uint16_t {
        SET_0 = 0x80,
        SET_1 = 0x40,
        SET_2 = 0x20,
        SET_3 = 0x10,
        SET_4 = 0x08,
    };
    uint16_t constraints;
    uint16_t level;
};

bool ExtractAvcSequenceParameterSet(std::vector<char>&& bitstream, AvcSequenceParameterSet* sps);

bool TestAvcStreamProfileLevel(const android::C2ProfileLevelStruct& profile_level, std::vector<char>&& bitstream, std::string* message);
