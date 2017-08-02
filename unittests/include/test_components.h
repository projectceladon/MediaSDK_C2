/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

// Collection of binary buffers hashes.
// The purpose is to check if encoder outputs differ between runs or the same.
class BinaryChunks
{
public:
    void PushBack(const uint8_t* data, size_t length)
    {
        std::basic_string<uint8_t> s(data, length);
        data_.emplace_back(std::hash<std::basic_string<uint8_t>>()(s));
    }
    bool operator==(const BinaryChunks& other)
    {
        return data_ == other.data_;
    }
    bool operator!=(const BinaryChunks& other)
    {
        return data_ != other.data_;
    }
private:
    std::vector<size_t> data_; // std::hash results
};
