/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <vector>
#include <fstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>

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

// Writes binary buffers to file.
class BinaryWriter
{
public:
    // File named <name> is created/overwritten in: ./<folders[0]>/.../<folders[N-1]>
    // Folders are created if missing.
    // So if a file with path ./Folder1/Folder2/File.txt is supposed to be written,
    // then it should be passed as: BinaryWriter( { "Folder1", "Folder2" }, "File.txt")
    BinaryWriter(const std::vector<std::string>& folders, const std::string& name);

public:
    void Write(const uint8_t* data, size_t length)
    {
        stream_.write((const char*)data, length);
    }

    static void Enable(bool enable)
    {
        enabled_ = enable;
    }
private:
    static bool enabled_;
    std::ofstream stream_;
};

// BinaryTester descendant simplifying BinaryWriter constructing for gtest tests
// It automatically gathered folders structure as ./<test_case_name>/<test_name>
class GTestBinaryWriter : public BinaryWriter
{
public:
    GTestBinaryWriter(const std::string& name);
    // This overload helps shorten complicated file names construction.
    GTestBinaryWriter(const std::ostringstream& stream)
        : GTestBinaryWriter(stream.str()) { }

private:
    static std::vector<std::string> GetTestFolders();
};
