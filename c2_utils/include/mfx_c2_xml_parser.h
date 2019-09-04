/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

/*
 * Copyright 2017, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
// Original source: ANDROID_TREE/frameworks/av/media/libstagefright/xmlparser

#include "mfx_c2_defs.h"
#include <map>

class MfxXmlParser
{
public:
    MfxXmlParser();
    ~MfxXmlParser() {}
    c2_status_t parseConfig(const char *path);
    C2Component::kind_t getKind(const char *name);
    C2String getMediaType(const char *name);
    bool dumpOutputEnabled(const char *name);

private:

    typedef std::map<C2String, C2String> AttributeMap;

    struct CodecProperties {
        bool isEncoder;    // whether this codec is an encoder or a decoder
        size_t order;      // order of appearance in the file (starting from 0)
        std::map<C2String, AttributeMap> typeMap;   // map of types supported by this codec
        bool dump_output{false};
    };

    enum Section {
        SECTION_TOPLEVEL,
        SECTION_DECODERS,
        SECTION_DECODER,
        SECTION_ENCODERS,
        SECTION_ENCODER,
        SECTION_INCLUDE,
    };

    c2_status_t parse(const char *path);

    c2_status_t include(const char **attrs);

    c2_status_t addMediaCodecFromAttributes(bool encoder, const char **attrs);

    c2_status_t addDiagnostics(const char **attrs);

    void startElementHandler(const char *name, const char **attrs);
    void endElementHandler(const char *name);

    static void StartElementHandlerWrapper(void *me, const char *name, const char **attrs);

    static void EndElementHandlerWrapper(void *me, const char *name);

    c2_status_t parsing_status_; // used for internal error handling, required by EXPAT callback
    Section current_section_;
    size_t codec_counter_;
    C2String href_base_;
    std::vector<Section> section_stack_;
    std::map<C2String, CodecProperties> codec_map_;
    std::map<C2String, CodecProperties>::iterator current_codec_;
    std::map<C2String, AttributeMap>::iterator current_type_;
};
