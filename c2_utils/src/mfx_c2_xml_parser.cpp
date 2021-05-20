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
    parsing_status_(C2_NO_INIT),
    current_section_(SECTION_TOPLEVEL),
    codec_counter_(0),
    href_base_(),
    section_stack_(),
    codec_map_(),
    current_codec_(),
    current_type_()
{}

c2_status_t MfxXmlParser::parseConfig(const char* path) {

    MFX_DEBUG_TRACE_FUNC;

    parsing_status_ = C2_OK;
    return parse(path);
}

C2String MfxXmlParser::getMediaType(const char* name) {

    MFX_DEBUG_TRACE_FUNC;

    auto codec = codec_map_.find(name);
    if (codec == codec_map_.end()) {
        MFX_DEBUG_TRACE_STREAM("codec " << name << "wasn't found");
        return C2String("");
    }
    return codec->second.typeMap.begin()->first;
}

C2Component::kind_t MfxXmlParser::getKind(const char *name) {
    MFX_DEBUG_TRACE_FUNC;

    auto codec = codec_map_.find(name);
    if (codec == codec_map_.end()) {
        MFX_DEBUG_TRACE_STREAM("codec " << name << "wasn't found");
        return KIND_OTHER;
    }
    return codec->second.isEncoder ? KIND_ENCODER : KIND_DECODER;
}

bool MfxXmlParser::dumpOutputEnabled(const char *name) {

    MFX_DEBUG_TRACE_FUNC;

    auto codec = codec_map_.find(name);
    if (codec == codec_map_.end()) {
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

    current_codec_ = codec_map_.find(name);
    if (current_codec_ == codec_map_.end()) {
        // create a new codec in codec_map_
        current_codec_ = codec_map_.insert(std::pair<C2String, CodecProperties>(name, CodecProperties())).first;
        if (type != nullptr) {
            current_type_ = current_codec_->second.typeMap.insert(std::pair<C2String, AttributeMap>(type, AttributeMap())).first;
        }
        current_codec_->second.isEncoder = encoder;
        current_codec_->second.order = codec_counter_++;
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

    current_codec_->second.dump_output = dump_output;
    return C2_OK;
}

void MfxXmlParser::startElementHandler(const char* name, const char** attrs) {

    MFX_DEBUG_TRACE_FUNC;

    if (parsing_status_ != C2_OK) {
        return;
    }

    if (strcmp(name, "Include") == 0) {
        parsing_status_ = include(attrs);
        if (parsing_status_ == C2_OK) {
            section_stack_.push_back(current_section_);
            current_section_ = SECTION_INCLUDE;
        }
        return;
    }

    switch (current_section_) {
        case SECTION_TOPLEVEL:
        {
            if (strcmp(name, "Decoders") == 0) {
                current_section_ = SECTION_DECODERS;
            } else if (strcmp(name, "Encoders") == 0) {
                current_section_ = SECTION_ENCODERS;
            }
            break;
        }

        case SECTION_DECODERS:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                parsing_status_ =
                    addMediaCodecFromAttributes(false /* encoder */, attrs);

                current_section_ = SECTION_DECODER;
            }
            break;
        }

        case SECTION_ENCODERS:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                parsing_status_ =
                    addMediaCodecFromAttributes(true /* encoder */, attrs);

                current_section_ = SECTION_ENCODER;
            }
            break;
        }

        case SECTION_DECODER:
        case SECTION_ENCODER:
        {
            if (strcmp(name, "Diagnostics") == 0) {
                parsing_status_ =
                    addDiagnostics(attrs);
            }
            break;
        }

        default:
            break;
    }

}

void MfxXmlParser::endElementHandler(const char* name) {
    if (parsing_status_ != C2_OK) {
        return;
    }

    switch (current_section_) {

        case SECTION_DECODERS:
        {
            if (strcmp(name, "Decoders") == 0) {
                current_section_ = SECTION_TOPLEVEL;
            }
            break;
        }

        case SECTION_ENCODERS:
        {
            if (strcmp(name, "Encoders") == 0) {
                current_section_ = SECTION_TOPLEVEL;
            }
            break;
        }

        case SECTION_DECODER:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                current_section_ = SECTION_DECODERS;
            }
            break;
        }

        case SECTION_ENCODER:
        {
            if (strcmp(name, "MediaCodec") == 0) {
                current_section_ = SECTION_ENCODERS;
            }
            break;
        }

        case SECTION_INCLUDE:
        {
            if ((strcmp(name, "Include") == 0) && (section_stack_.size() > 0)) {
                current_section_ = section_stack_.back();
                section_stack_.pop_back();
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
    while (parsing_status_ == C2_OK) {
        void* buff = ::XML_GetBuffer(parser, BUFF_SIZE);
        if (buff == nullptr) {
            MFX_DEBUG_TRACE_STREAM("failed in call to XML_GetBuffer()");
            parsing_status_ = C2_BAD_VALUE;
            break;
        }

        int bytes_read = fread(buff, 1, BUFF_SIZE, file);
        if (bytes_read < 0) {
            MFX_DEBUG_TRACE_STREAM("failed in call to read");
            parsing_status_ = C2_BAD_VALUE;
            break;
        }

        XML_Status status = ::XML_ParseBuffer(parser, bytes_read, bytes_read == 0);
        if (status != XML_STATUS_OK) {
            MFX_DEBUG_TRACE_STREAM("malformed " << ::XML_ErrorString(::XML_GetErrorCode(parser)));
            parsing_status_ = C2_BAD_VALUE;
            break;
        }

        if (bytes_read == 0) {
            break;
        }
    }

    ::XML_ParserFree(parser);
    fclose(file);

    MFX_DEBUG_TRACE__android_c2_status_t(parsing_status_);
    return parsing_status_;
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
    filename.insert(0, href_base_);

    return parse(filename.c_str());
}
