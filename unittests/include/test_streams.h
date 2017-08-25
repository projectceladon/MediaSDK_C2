/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <vector>

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
    bool Read(const Slicing& slicing, StreamDescription::Region* region, bool* header);

    bool ContainsHeader(const StreamDescription::Region& region);

    bool Seek(size_t pos);

private:
    const StreamDescription& stream_;
    std::vector<char>::const_iterator pos_;
};

inline bool StreamReader::Read(const Slicing& slicing, StreamDescription::Region* region, bool* header)
{
    bool res = true;
    if(pos_ < stream_.data.end()) {
        switch(slicing.GetType()) {
            case Slicing::Type::NalUnit: {
                const std::vector<char> delim = { 0, 0, 0, 1 };

                auto found = stream_.data.end();
                if(size_t(stream_.data.end() - pos_) >= delim.size()) {
                    std::search(pos_ + delim.size(), stream_.data.end(), delim.begin(), delim.end());
                }
                region->offset = pos_ - stream_.data.begin();
                region->size = found - pos_;
                *header = ContainsHeader(*region);
                pos_ = found;
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
