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
    uint32_t getConcurrentInstances(const char *name);
    bool dumpOutputEnabled(const char *name);

private:

    typedef std::map<C2String, C2String> AttributeMap;

    struct CodecProperties {
        bool isEncoder;    // whether this codec is an encoder or a decoder
        size_t order;      // order of appearance in the file (starting from 0)
        std::map<C2String, AttributeMap> typeMap;   // map of types supported by this codec
        bool dump_output{false};
        uint32_t concurrentInstance{0};
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

    c2_status_t addLimits(const char **attrs);

    void startElementHandler(const char *name, const char **attrs);
    void endElementHandler(const char *name);

    static void StartElementHandlerWrapper(void *me, const char *name, const char **attrs);

    static void EndElementHandlerWrapper(void *me, const char *name);

    c2_status_t m_parsingStatus; // used for internal error handling, required by EXPAT callback
    Section m_currentSection;
    size_t m_uCodecCounter;
    C2String m_hrefBase;
    std::vector<Section> m_sectionStack;
    std::map<C2String, CodecProperties> m_codecMap;
    std::map<C2String, CodecProperties>::iterator m_currentCodec;
    std::map<C2String, AttributeMap>::iterator m_currentType;
};
