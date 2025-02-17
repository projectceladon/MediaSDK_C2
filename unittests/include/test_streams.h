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
#include <C2Config.h>
#include "mfxstructures.h"

struct StreamDescription
{
    const char* name;
    uint32_t fourcc;
    struct Region
    {
        size_t offset; // current byte offset in the stream
        uint8_t bit_offset; // current bit offset in the current byte
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
    std::vector<uint32_t> frames_crc32_nv12; // Crc32 checksums for every frame,
    // stored in displayed order.

    size_t gop_size; // Here gop is meant as pictures group could be decoded independently,
    // no refs to other groups, used for seek.

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
    static std::unique_ptr<StreamReader> Create(const std::vector<const StreamDescription*>& streams);

    virtual ~StreamReader() = default;
    // reads next stream chunk specified in slicing
    virtual bool Read(const Slicing& slicing, StreamDescription::Region* region, bool* header, size_t* start_code_len = nullptr) = 0;

    virtual size_t GetPos() const = 0;

    virtual bool Seek(size_t pos) = 0;

    virtual bool EndOfStream() = 0;

    virtual std::vector<char> GetRegionContents(StreamDescription::Region region) const = 0;
};

class SingleStreamReader : public StreamReader
{
public:
    SingleStreamReader(const StreamDescription* stream):
        stream_(stream),
        pos_(stream->data.begin())
        {}
public:
    virtual bool Read(const Slicing& slicing, StreamDescription::Region* region, bool* header, size_t* start_code_len = nullptr) override;

    virtual size_t GetPos() const override;

    virtual bool Seek(size_t pos) override;

    virtual bool EndOfStream() override;

    virtual std::vector<char> GetRegionContents(StreamDescription::Region region) const override;

private:
    bool ContainsHeader(const StreamDescription::Region& region);

    bool ContainsSlice(const StreamDescription::Region& region, size_t start_code_len);

    bool ParseIVFHeader(size_t* frame_size);

private:
    const StreamDescription* stream_;
    std::vector<char>::const_iterator pos_;

    static const uint8_t NAL_UNITTYPE_BITS_H264 = 0x1f;

    static const uint8_t NAL_UT_SLICE = 1;
    static const uint8_t NAL_UT_IDR_SLICE = 5;

    static const uint8_t NAL_UNITTYPE_BITS_H265 = 0x7e;
    static const uint8_t NAL_UNITTYPE_SHIFT_H265 = 1;

    static const uint8_t NAL_UT_CODED_SLICE_TRAIL_N    = 0;
    static const uint8_t NAL_UT_CODED_SLICE_TRAIL_R    = 1;
    static const uint8_t NAL_UT_CODED_SLICE_TSA_N      = 2;
    static const uint8_t NAL_UT_CODED_SLICE_TLA_R      = 3;
    static const uint8_t NAL_UT_CODED_SLICE_STSA_N     = 4;
    static const uint8_t NAL_UT_CODED_SLICE_STSA_R     = 5;
    static const uint8_t NAL_UT_CODED_SLICE_RADL_N     = 6;
    static const uint8_t NAL_UT_CODED_SLICE_RADL_R     = 7;
    static const uint8_t NAL_UT_CODED_SLICE_RASL_N     = 8;
    static const uint8_t NAL_UT_CODED_SLICE_RASL_R     = 9;
    static const uint8_t NAL_UT_CODED_SLICE_BLA_W_LP   = 16;
    static const uint8_t NAL_UT_CODED_SLICE_BLA_W_RADL = 17;
    static const uint8_t NAL_UT_CODED_SLICE_BLA_N_LP   = 18;
    static const uint8_t NAL_UT_CODED_SLICE_IDR_W_RADL = 19;
    static const uint8_t NAL_UT_CODED_SLICE_IDR_N_LP   = 20;
    static const uint8_t NAL_UT_CODED_SLICE_CRA        = 21;
};

class CombinedStreamReader : public StreamReader
{
public:
    CombinedStreamReader(std::vector<const StreamDescription*> streams):
        streams_(streams)
    {
        std::transform(streams.begin(), streams.end(), std::back_inserter(readers_),
            [] (const StreamDescription* stream) { return SingleStreamReader(stream); } );
    }
    virtual ~CombinedStreamReader() = default;
public:
    virtual bool Read(const Slicing& slicing, StreamDescription::Region* region, bool* header, size_t* start_code_len = nullptr) override;

    virtual size_t GetPos() const override;

    virtual bool Seek(size_t pos) override;

    virtual bool EndOfStream() override;

    virtual std::vector<char> GetRegionContents(StreamDescription::Region region) const override;

private:
    std::vector<const StreamDescription*> streams_;
    std::vector<SingleStreamReader> readers_;
    size_t active_stream_offset_ { 0 };
    size_t active_stream_index_ { 0 };
};

inline bool SingleStreamReader::Read(const Slicing& slicing, StreamDescription::Region* region, bool* header, size_t* start_code_len)
{
    bool res = true;
    if(pos_ < stream_->data.end()) {
        switch(slicing.GetType()) {
            case Slicing::Type::NalUnit: {
                if (stream_->fourcc != MFX_CODEC_VP9) { // do nothing for VP9 stream
                    const std::vector<std::vector<char>> delims =
                        { { 0, 0, 0, 1 }, { 0, 0, 1 } };

                    std::vector<char>::const_iterator find_from = pos_;
                    for (const auto& delim : delims) {
                        std::vector<char>::const_iterator found =
                            std::search(pos_, stream_->data.end(), delim.begin(), delim.end());
                        if (found == pos_) { // this is current beginning of current nal unit, skip it
                            find_from = pos_ + delim.size();
                            break;
                        }
                    }

                    std::vector<char>::const_iterator nearest_delim = stream_->data.end();
                    for (const auto& delim : delims) {
                        std::vector<char>::const_iterator found =
                            std::search(find_from, stream_->data.end(), delim.begin(), delim.end());
                        if (found < nearest_delim) {
                            nearest_delim = found;
                        }
                    }
                    region->offset = pos_ - stream_->data.begin();
                    region->size = nearest_delim - pos_;
                    *header = ContainsHeader(*region);
                    if (nullptr != start_code_len) {
                        *start_code_len = find_from - pos_;
                    }
                    pos_ = nearest_delim;
                } else {
                    res = false;
                }
                break;
            }
            case Slicing::Type::Frame: {
                if (stream_->fourcc == MFX_CODEC_VP9) {
                    size_t current_frame_size = 0;
                    res = ParseIVFHeader(&current_frame_size);
                    region->offset = pos_ - stream_->data.begin();
                    region->size = current_frame_size;
                    pos_ += current_frame_size;
                } else {
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
                }
                break;
            }
            case Slicing::Type::Fixed: {
                region->offset = pos_ - stream_->data.begin();
                region->size = std::min<size_t>(stream_->data.end() - pos_, slicing.GetSize());
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

inline bool SingleStreamReader::ContainsHeader(const StreamDescription::Region& region)
{
    return stream_->sps.Intersects(region) || stream_->pps.Intersects(region);
}

inline bool SingleStreamReader::ContainsSlice(const StreamDescription::Region& region, size_t start_code_len)
{
    bool is_slice = false;
    uint8_t nal_unit_type = 0;

    if (region.size >= start_code_len + 1) {
        char header_byte = stream_->data[region.offset + start_code_len]; // first byte after start code

        switch(stream_->fourcc) {
            case MFX_CODEC_AVC:
                nal_unit_type = (uint8_t)header_byte & NAL_UNITTYPE_BITS_H264;
                if (nal_unit_type == NAL_UT_SLICE || nal_unit_type == NAL_UT_IDR_SLICE) {
                    is_slice = true;
                }
                break;

            case MFX_CODEC_HEVC:
                nal_unit_type = ((uint8_t)header_byte & NAL_UNITTYPE_BITS_H265) >> NAL_UNITTYPE_SHIFT_H265;
                switch(nal_unit_type) {
                    case NAL_UT_CODED_SLICE_TRAIL_N:
                    case NAL_UT_CODED_SLICE_TRAIL_R:
                    case NAL_UT_CODED_SLICE_TSA_N:
                    case NAL_UT_CODED_SLICE_TLA_R:
                    case NAL_UT_CODED_SLICE_STSA_N:
                    case NAL_UT_CODED_SLICE_STSA_R:
                    case NAL_UT_CODED_SLICE_RADL_N:
                    case NAL_UT_CODED_SLICE_RADL_R:
                    case NAL_UT_CODED_SLICE_RASL_N:
                    case NAL_UT_CODED_SLICE_RASL_R:
                    case NAL_UT_CODED_SLICE_BLA_W_LP:
                    case NAL_UT_CODED_SLICE_BLA_W_RADL:
                    case NAL_UT_CODED_SLICE_BLA_N_LP:
                    case NAL_UT_CODED_SLICE_IDR_W_RADL:
                    case NAL_UT_CODED_SLICE_IDR_N_LP:
                    case NAL_UT_CODED_SLICE_CRA:
                        is_slice = true;
                        break;
                    default:
                        break;
                };
                break;

            default:
                break;
        }
    }
    return is_slice;
}

inline bool SingleStreamReader::ParseIVFHeader(size_t* frame_size)
{
    if (pos_ == stream_->data.begin()) {
        // check IVF Header
        if ((stream_->data.size() >= 32) &&
            (stream_->data[0] == 0x44 && // D
             stream_->data[1] == 0x4B && // K
             stream_->data[2] == 0x49 && // I
             stream_->data[3] == 0x46)) {// F
            pos_ += 32;
        } else {
            return false;
        }
    }
    if ((stream_->data.end() - pos_) >= 12) { // read frame header
        // read actual frame size - first 4 byte
        char tmp_array[4];
        for (uint8_t i = 0; i < 4; i++) {
            tmp_array[i] = *pos_;
            pos_++;
        }
        *frame_size = *reinterpret_cast<uint32_t*>(tmp_array);
        // skip header (12 - 4) byte
        pos_ += 8;
    } else {
        return false;
    }
    return true;
}

inline size_t SingleStreamReader::GetPos() const
{
    return pos_ - stream_->data.begin();
}

inline bool SingleStreamReader::Seek(size_t pos)
{
    bool res = true;
    if(pos <= stream_->data.size()) {
        pos_ = stream_->data.begin() + pos;
    } else {
        res = false;
    }
    return res;
}

inline bool SingleStreamReader::EndOfStream()
{
    return pos_ == stream_->data.end();
}

inline std::vector<char> SingleStreamReader::GetRegionContents(StreamDescription::Region region) const
{
    size_t begin = std::min(region.offset, stream_->data.size());
    size_t end = std::min(region.offset + region.size, stream_->data.size());

    return std::vector<char>(stream_->data.begin() + begin,
        stream_->data.begin() + end);
}

namespace HeaderParser
{
class AvcSequenceParameterSet
{
public:
    uint16_t profile_;
    enum Constraint : uint16_t {
        SET_0 = 0x80,
        SET_1 = 0x40,
        SET_2 = 0x20,
        SET_3 = 0x10,
        SET_4 = 0x08,
    };
    uint16_t constraints_;
    uint16_t level_;
    float frame_rate_;
    bool ExtractSequenceParameterSet(std::vector<char>&& bitstream);
private:
    void ParseVUI(const std::vector<char>& data, StreamDescription::Region* region);
};

class HevcSequenceParameterSet
{
public:
    uint8_t profile_;
    enum Profiles : uint8_t {
        HEVC_MAIN    = 1,
        HEVC_MAIN_10 = 2,
        HEVC_MAIN_SP = 1,
        HEVC_REXT    = 4,
        HEVC_REXT_HT = 5,
        HEVC_MAIN_MV = 6,
        HEVC_MAIN_SC = 7,
        HEVC_MAIN_3D = 8,
        HEVC_SCC     = 9,
        HEVC_REXT_SC = 10,
    };
    uint16_t level_;
    float frame_rate_;
    bool ExtractSequenceParameterSet(std::vector<char>&& bitstream);
private:
    uint8_t max_sub_layers_minus1_;
    uint32_t log2_max_pic_order_cnt_lsb_minus4_;
    void ParsePTL(const std::vector<char>& data, StreamDescription::Region* region);
    void ParseSLD(const std::vector<char>& data, StreamDescription::Region* region);
    void ParseSTRPS(const std::vector<char>& data, StreamDescription::Region* region);
    void ParseLTRPS(const std::vector<char>& data, StreamDescription::Region* region);
    void ParseVUI(const std::vector<char>& data, StreamDescription::Region* region);
    bool ProfileMatches(uint8_t real, uint8_t expected, uint32_t flag)
    {
        return (real == expected || (flag & (1 << (31 - expected))));
    }
};
}

bool TestAvcStreamProfileLevel(const C2ProfileLevelStruct& profile_level, std::vector<char>&& bitstream, std::string* message);

bool TestHevcStreamProfileLevel(const C2ProfileLevelStruct& profile_level, std::vector<char>&& bitstream, std::string* message);
