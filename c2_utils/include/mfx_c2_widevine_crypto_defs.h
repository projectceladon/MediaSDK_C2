// Copyright (c) 2017-2024 Intel Corporation
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
#include <array>

#define PROTECTED_DATA_BUFFER_MAGIC (0UL | ('E' << 24) | ('B' << 16) | ('D' << 8) | 'P')
using IV = std::array<uint8_t, 16>;

typedef unsigned int VAGenericID;
typedef VAGenericID VAContextID;


typedef enum OEMCryptoCipherMode {
  OEMCrypto_CipherMode_CTR,
  OEMCrypto_CipherMode_CBC,
  OEMCrypto_CipherMode_MaxValue = OEMCrypto_CipherMode_CBC,
} OEMCryptoCipherMode;

typedef struct {
    size_t block_offset;
    IV current_iv;
    size_t data_length;
    size_t clear_bytes;
    size_t encrypted_bytes;
    size_t pattern_clear;
    size_t pattern_encrypted;
} packet_info;

typedef struct {
    uint32_t pr_magic;
    uint32_t app_id;
    size_t num_packet_data;
    size_t sample_size;
    OEMCryptoCipherMode cipher_mode;
    uint8_t hw_key_data[16];
    packet_info* packet_data;
} HUCVideoBuffer;