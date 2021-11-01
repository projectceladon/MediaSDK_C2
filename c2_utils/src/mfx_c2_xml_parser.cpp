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

#include "mfx_c2_xml_parser.h"
#include "mfx_c2_debug.h"
#include <expat.h>

MfxXmlParser::MfxXmlParser():
    m_parsingStatus(C2_NO_INIT),
    m_currentSection(SECTION_TOPLEVEL),
    m_uCodecCounter(0),
    m_hrefBase(),
    m_sectionStack(),
    m_codecMap(),
    m_currentCodec(),
    m_currentType()
{}

c2_status_t MfxXmlParser::parseConfig(const char* path) {

    MFX_DEBUG_TRACE_FUNC;

    m_parsingStatus = C2_OK;
    return parse(path);
}

C2String MfxXmlParser::getMediaType(const char* name) {

    MFX_DEBUG_TRACE_FUNC;

    auto codec = m_codecMap.find(name);
    if (codec == m_codecMap.end()) {
        MFX_DEBUG_TRACE_STREAM("codec " << name << "wasn't found");
        return C2String("");
    }
    return codec->second.typeMap.begin()->first;
}

C2Component::kind_t MfxXmlParser::getKind(const char *name) {
    MFX_DEBUG_TRACE_FUNC;

    auto codec = m_codecMap.find(name);
    if (codec == m_codecMap.end()) {
        MFX_DEBUG_TRACE_STREAM("codec " << name << "wasn't found");
        return KIND_OTHER;
    }
    return codec->second.isEncoder ? KIND_ENCODER : KIND_DECODER;
}

bool MfxXmlParser::dumpOutputEnabled(const char *name) {

    MFX_DEBUG_TRACE_FUNC;

    auto codec = m_codecMap.find(name);
    if (codec == m_codecMap.end()) {
        MFX_DEBUG_TRACE_STREAM("codec " << name << "wasn't found");
        return false;
    }
    return codec->second.dump_output;
}

c2_status_t MfxXmlParser::addMediaCodecFromAttributes(bool encoder, const char** attrs) {

    MFX_DEBUG_TRACE_FUNC;

    const char* name = nullptr;
    const char* type = nullptr;

    size_t i = 0;
    while (attrs[i] != nullptr) {
        if (strcmp(attrs[i], "name") == 0) {
            if (attrs[++i] == nullptr) {
                MFX_DEBUG_TRACE_STREAM("name is null");
                return C2_BAD_VALUE;
            }
            name = attrs[i];
        } else if (strcmp(attrs[i], "type") == 0) {
            if (attrs[++i] == nullptr) {
                MFX_DEBUG_TRACE_STREAM("type is null");
                return C2_BAD_VALUE;
            }
            type = attrs[i];
        } else {
            MFX_DEBUG_TRACE_STREAM("unrecognized attribute: " << attrs[i]);
            return C2_BAD_VALUE;
        }
        i++;
    }

    if (name == nullptr) {
        MFX_DEBUG_TRACE_STREAM("name not found");
        return C2_BAD_VALUE;
    }

    m_currentCodec = m_codecMap.find(name);
    if (m_currentCodec == m_codecMap.end()) {
        // create a new codec in m_codecMap
        m_currentCodec = m_codecMap.insert(std::pair<C2String, CodecProperties>(name, CodecProperties())).first;
        if (type != nullptr) {
            m_currentType = m_currentCodec->second.typeMap.insert(std::pair<C2String, AttributeMap>(type, AttributeMap())).first;
        }
        m_currentCodec->second.isEncoder = encoder;
        m_currentCodec->second.order = m_uCodecCounter++;
    } else {
        // existing codec name
        MFX_DEBUG_TRACE_STREAM("adding existing codec");
        return C2_BAD_VALUE;
    }

    return C2_OK;
}

static bool parseBoolean(const char* s) {
    return strcasecmp(s, "y") == 0 ||
        strcasecmp(s, "yes") == 0 ||
        strcasecmp(s, "t") == 0 ||
        strcasecmp(s, "true") == 0 ||
        strcasecmp(s, "1") == 0;
}

c2_status_t MfxXmlParser::addDiagnostics(const char **attrs) {
    MFX_DEBUG_TRACE_FUNC;

    bool dump_output{false};

    size_t i = 0;
    while (attrs[i] != nullptr) {
        if (strcmp(attrs[i], "dumpOutput") == 0) {
            if (attrs[++i] == nullptr) {
                MFX_DEBUG_TRACE_STREAM("dumpOutput is null");
                return C2_BAD_VALUE;
            }
            dump_output = parseBoolean(attrs[i]);
            MFX_DEBUG_TRACE_STREAM("dumpOutput value " << attrs[i] << " parsed to " << (int)dump_output);
        } else {
            MFX_DEBUG_TRACE_STREAM("unrecognized attribute: " << attrs[i]);
            return C2_BAD_VALUE;
        }
        ++i;
    }

    m_currentCodec->second.dump_output = dump_output;
    return C2_OK;
}

void MfxXmlParser::startElementHandler(const char* name, const char** attrs) {

    MFX_DEBUG_TRACE_FUNC;

    if (m_parsingStatus != C2_OK) {
        return;
    }

    if (strcmp(name, "Include") == 0) {
        m_parsingStatus = include(attrs);
        if (m_parsingStatus == C2_OK) {
            m_sectionStack.push_back(m_currentSection);
            m_currentSection = SECTION_INCLUDE;
        }
        return;
    }

    switch (m_currentSection) {
        case SECTION_TOPLEVEL:
        {
            if (strcmp(name, "Decoders") == 0) {
                m_currentSection = SECTION_DECODERS;
            } else if (strcmp(name, "Encoders") == 0) {
                m_currentSection = SECTION_ENCODERS;
            }
            break;
        }

        case SECTION_DECODERS:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                m_parsingStatus =
                    addMediaCodecFromAttributes(false /* encoder */, attrs);

                m_currentSection = SECTION_DECODER;
            }
            break;
        }

        case SECTION_ENCODERS:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                m_parsingStatus =
                    addMediaCodecFromAttributes(true /* encoder */, attrs);

                m_currentSection = SECTION_ENCODER;
            }
            break;
        }

        case SECTION_DECODER:
        case SECTION_ENCODER:
        {
            if (strcmp(name, "Diagnostics") == 0) {
                m_parsingStatus =
                    addDiagnostics(attrs);
            }
            break;
        }

        default:
            break;
    }

}

void MfxXmlParser::endElementHandler(const char* name) {
    if (m_parsingStatus != C2_OK) {
        return;
    }

    switch (m_currentSection) {

        case SECTION_DECODERS:
        {
            if (strcmp(name, "Decoders") == 0) {
                m_currentSection = SECTION_TOPLEVEL;
            }
            break;
        }

        case SECTION_ENCODERS:
        {
            if (strcmp(name, "Encoders") == 0) {
                m_currentSection = SECTION_TOPLEVEL;
            }
            break;
        }

        case SECTION_DECODER:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                m_currentSection = SECTION_DECODERS;
            }
            break;
        }

        case SECTION_ENCODER:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                m_currentSection = SECTION_ENCODERS;
            }
            break;
        }

        case SECTION_INCLUDE:
        {
            if ((strcmp(name, "Include") == 0) && (m_sectionStack.size() > 0)) {
                m_currentSection = m_sectionStack.back();
                m_sectionStack.pop_back();
            }
            break;
        }

        default:
            break;
    }

}

// static
void MfxXmlParser::StartElementHandlerWrapper(void* me, const char* name, const char** attrs) {
    static_cast<MfxXmlParser*>(me)->startElementHandler(name, attrs);
}

// static
void MfxXmlParser::EndElementHandlerWrapper(void* me, const char* name) {
    static_cast<MfxXmlParser*>(me)->endElementHandler(name);
}

c2_status_t MfxXmlParser::parse(const char* path) {

    MFX_DEBUG_TRACE_FUNC;

    FILE* file = fopen(path, "r");

    if (file == nullptr) {
        MFX_DEBUG_TRACE_STREAM("unable to open media codecs configuration xml file: " << path);
        return C2_NOT_FOUND;
    }

    XML_Parser parser = ::XML_ParserCreate(nullptr);
    if (parser == nullptr) {
        MFX_DEBUG_TRACE_STREAM("XmlParserCreate() failed");
        ::XML_ParserFree(parser);
        fclose(file);
        return C2_NO_INIT;
    }

    ::XML_SetUserData(parser, this);
    ::XML_SetElementHandler(
            parser, StartElementHandlerWrapper, EndElementHandlerWrapper);

    static constexpr int BUFF_SIZE = 512;
    while (m_parsingStatus == C2_OK) {
        void* buff = ::XML_GetBuffer(parser, BUFF_SIZE);
        if (buff == nullptr) {
            MFX_DEBUG_TRACE_STREAM("failed in call to XML_GetBuffer()");
            m_parsingStatus = C2_BAD_VALUE;
            break;
        }

        int bytes_read = fread(buff, 1, BUFF_SIZE, file);
        if (bytes_read < 0) {
            MFX_DEBUG_TRACE_STREAM("failed in call to read");
            m_parsingStatus = C2_BAD_VALUE;
            break;
        }

        XML_Status status = ::XML_ParseBuffer(parser, bytes_read, bytes_read == 0);
        if (status != XML_STATUS_OK) {
            MFX_DEBUG_TRACE_STREAM("malformed " << ::XML_ErrorString(::XML_GetErrorCode(parser)));
            m_parsingStatus = C2_BAD_VALUE;
            break;
        }

        if (bytes_read == 0) {
            break;
        }
    }

    ::XML_ParserFree(parser);
    fclose(file);

    MFX_DEBUG_TRACE__android_c2_status_t(m_parsingStatus);
    return m_parsingStatus;
}

c2_status_t MfxXmlParser::include(const char** attrs) {

    MFX_DEBUG_TRACE_FUNC;

    const char* href = nullptr;
    size_t i = 0;
    while (attrs[i] != nullptr) {
        if (strcmp(attrs[i], "href") == 0) {
            if (attrs[++i] == nullptr) {
                return C2_BAD_VALUE;
            }
            href = attrs[i];
        } else {
            MFX_DEBUG_TRACE_STREAM("unrecognized attribute: " << attrs[i]);
            return C2_BAD_VALUE;
        }
        i++;
    }

    if (!href) return C2_BAD_VALUE;

    // for simplicity, file names can only contain
    // [a-zA-Z0-9_.] and must start with  media_codecs_ and end with .xml
    for (i = 0; href[i] != '\0'; i++) {
        if (href[i] == '.' || href[i] == '_' ||
                (href[i] >= '0' && href[i] <= '9') ||
                (href[i] >= 'A' && href[i] <= 'Z') ||
                (href[i] >= 'a' && href[i] <= 'z')) {
            continue;
        }
        MFX_DEBUG_TRACE_STREAM("invalid include file name: " << href);
        return C2_BAD_VALUE;
    }

    C2String filename = href;
    if (filename.compare(0, 13, "media_codecs_") != 0 ||
        filename.compare(filename.size() - 4, 4, ".xml") != 0) {
        MFX_DEBUG_TRACE_STREAM("invalid include file name: " << href);
        return C2_BAD_VALUE;
    }
    filename.insert(0, m_hrefBase);

    return parse(filename.c_str());
}
