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

#include <C2Component.h>
#include <C2Config.h>
#include "mfx_c2_param_reflector.h"

#define MFX_C2_COMPONENT_STORE_NAME "MfxC2ComponentStore"

#define CREATE_MFX_C2_COMPONENT_FUNC_NAME "MfxCreateC2Component"

#define MFX_C2_CONFIG_FILE_NAME "mfx_c2_store.conf"
#define MFX_C2_CONFIG_FILE_PATH "/vendor/etc"

#define MFX_C2_CONFIG_XML_FILE_NAME "media_codecs_intel_c2_video.xml"
#define MFX_C2_CONFIG_XML_FILE_PATH "/vendor/etc"

#define MFX_C2_DUMP_DIR "/data/local/traces"
#define MFX_C2_DUMP_DECODER_SUB_DIR "c2-intel-decoder"
#define MFX_C2_DUMP_ENCODER_SUB_DIR "c2-intel-encoder"
#define MFX_C2_DUMP_OUTPUT_SUB_DIR "output"
#define MFX_C2_DUMP_INPUT_SUB_DIR "input"

// dump when dump frames number > 0
#define DECODER_DUMP_OUTPUT_KEY "vendor.c2.dump.decoder.output.number"
// dump when property set to true
#define DECODER_DUMP_INPUT_KEY "vendor.c2.dump.decoder.input"
// dump when property set to true
#define ENCODER_DUMP_OUTPUT_KEY "vendor.c2.dump.encoder.output"
// dump when dump frames number > 0
#define ENCODER_DUMP_INPUT_KEY "vendor.c2.dump.encoder.input.number"

const c2_nsecs_t MFX_SECOND_NS = 1000000000; // 1e9

extern const size_t g_h264_profile_levels_count;
extern const C2ProfileLevelStruct g_h264_profile_levels[];

extern const size_t g_h265_profile_levels_count;
extern const C2ProfileLevelStruct g_h265_profile_levels[];

// TODO: Update this value if you need to add ExtBufHolder type
constexpr uint16_t g_max_num_ext_buffers = 2;
