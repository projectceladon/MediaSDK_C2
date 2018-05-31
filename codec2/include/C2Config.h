/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef C2CONFIG_H_
#define C2CONFIG_H_

#include <C2ParamDef.h>

/// \defgroup config Component configuration
/// @{

#ifndef DEFINE_C2_ENUM_VALUE_AUTO_HELPER
#define DEFINE_C2_ENUM_VALUE_AUTO_HELPER(name, type, prefix, ...)
#define DEFINE_C2_ENUM_VALUE_CUSTOM_HELPER(name, type, names, ...)
#endif

#define C2ENUM(name, type, ...) \
enum name : type { __VA_ARGS__ }; \
DEFINE_C2_ENUM_VALUE_AUTO_HELPER(name, type, NULL, __VA_ARGS__)

#define C2ENUM_CUSTOM_PREFIX(name, type, prefix, ...) \
enum name : type { __VA_ARGS__ }; \
DEFINE_C2_ENUM_VALUE_AUTO_HELPER(name, type, prefix, __VA_ARGS__)

#define C2ENUM_CUSTOM_NAMES(name, type, names, ...) \
enum name : type { __VA_ARGS__ }; \
DEFINE_C2_ENUM_VALUE_CUSTOM_HELPER(name, type, names, __VA_ARGS__)

enum C2ParamIndexKind : C2Param::type_index_t {
    /// domain
    kParamIndexDomain,

    /// configuration descriptors
    kParamIndexSupportedParams,
    kParamIndexRequiredParams,
    kParamIndexReadOnlyParams,
    kParamIndexRequestedInfos,

    /// latency
    kParamIndexLatency,

    // generic time behavior
    kParamIndexTemporal,

    /// port configuration
    kParamIndexMime,
    kParamIndexStreamCount,
    kParamIndexFormat,
    kParamIndexBlockPools,
    kParamIndexUsage,
    kParamIndexBitrate,

    kParamIndexMaxVideoSizeHint,
    kParamIndexVideoSizeTuning,
    kParamIndexFrameRate,

    kParamIndexCsd,
    kParamIndexPictureTypeMask,

    kParamIndexSampleRate,
    kParamIndexChannelCount,

    kParamIndexAacStreamFormat,

    // video info

    kParamIndexStructStart = 0x1,
    kParamIndexVideoSize,

    kParamIndexParamStart = 0x800,
};

C2ENUM(C2DomainKind, uint32_t,
    C2DomainVideo,
    C2DomainAudio,
    C2DomainOther = C2DomainAudio + 1
);

// read-only

typedef C2GlobalParam<C2Info, C2SimpleValueStruct<C2DomainKind>, kParamIndexDomain> C2ComponentDomainInfo;
// typedef C2GlobalParam<C2Info, C2Uint32Value, kParamIndexDomain> C2ComponentDomainInfo;

// read-only
typedef C2GlobalParam<C2Info, C2Uint32Array, kParamIndexSupportedParams> C2SupportedParamsInfo;

/// \todo do we define it as a param?
// read-only
typedef C2GlobalParam<C2Info, C2Uint32Array, kParamIndexRequiredParams> C2RequiredParamsInfo;

// read-only
typedef C2GlobalParam<C2Info, C2Uint32Array, kParamIndexReadOnlyParams> C2ReadOnlyParamsInfo;

// read-only
typedef C2GlobalParam<C2Info, C2Uint32Array, kParamIndexRequestedInfos> C2RequestedInfosInfo;

// read-only
//typedef C2GlobalParam<C2Info, C2Uint32Value, kParamIndexRequestedInfos> C2RequestedInfosInfo;

/// latency

typedef C2PortParam<C2Info, C2Uint32Value, kParamIndexLatency> C2PortLatencyInfo;

typedef C2GlobalParam<C2Info, C2Uint32Value, kParamIndexLatency> C2ComponentLatencyInfo;

/// \todo
typedef C2GlobalParam<C2Info, C2Uint32Value, kParamIndexTemporal> C2ComponentTemporalInfo;

/// port configuration

typedef C2PortParam<C2Tuning, C2StringValue, kParamIndexMime> C2PortMimeConfig;
constexpr char C2_NAME_INPUT_PORT_MIME_SETTING[]  = "mediatype.input";
constexpr char C2_NAME_OUTPUT_PORT_MIME_SETTING[] = "mediatype.output";

typedef C2PortParam<C2Tuning, C2Uint32Value, kParamIndexStreamCount> C2PortStreamCountConfig;

typedef C2StreamParam<C2Tuning, C2StringValue, kParamIndexMime> C2StreamMimeConfig;

C2ENUM(C2FormatKind, uint32_t,
    C2FormatCompressed,
    C2FormatAudio = 1,
    C2FormatVideo = 4,
)

typedef C2StreamParam<C2Tuning, C2Uint32Value, kParamIndexFormat> C2StreamFormatConfig;
constexpr char C2_NAME_INPUT_STREAM_FORMAT_SETTING[]  = "format.input";
constexpr char C2_NAME_OUTPUT_STREAM_FORMAT_SETTING[] = "format.output";

typedef C2PortParam<C2Tuning, C2Uint64Array, kParamIndexBlockPools> C2PortBlockPoolsTuning;

// read-only
typedef C2StreamParam<C2Tuning, C2Uint64Value, kParamIndexUsage> C2StreamUsageTuning;
constexpr char C2_NAME_INPUT_STREAM_USAGE_SETTING[] = "usage.input";

// encoder bitrate [IN]
typedef C2StreamParam<C2Tuning, C2Uint32Value, kParamIndexBitrate> C2BitrateTuning;
constexpr char C2_NAME_STREAM_BITRATE_SETTING[] = "coded.bitrate";

typedef C2StreamParam<C2Info, C2BlobValue, kParamIndexCsd> C2StreamCsdInfo;

C2ENUM(C2PictureTypeMask, uint32_t,
    C2PictureTypeKeyFrame = (1u << 0),
)

typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexPictureTypeMask> C2StreamPictureTypeMaskInfo;

// audio encoder sample rate [IN]
typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexSampleRate> C2StreamSampleRateInfo;
constexpr char C2_NAME_STREAM_SAMPLE_RATE_SETTING[] = "raw.sample-rate";

// audio encoder channel count [IN]
typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexChannelCount> C2StreamChannelCountInfo;
constexpr char C2_NAME_STREAM_CHANNEL_COUNT_SETTING[] = "raw.channel-count";

// aac encoder stream format [IN]
C2ENUM(C2AacStreamFormatKind, uint32_t,
    C2AacStreamFormatRaw,
    C2AacStreamFormatAdts,
    C2AacStreamFormatOther,
)
typedef C2StreamParam<C2Info, C2Uint32Value, kParamIndexAacStreamFormat> C2StreamAacFormatInfo;
constexpr char C2_NAME_STREAM_AAC_FORMAT_SETTING[] = "coded.aac-stream-format";


/*
   Component description fields:

// format (video/compressed/audio/other-do we need other?) per stream

// likely some of these are exposed as separate settings:

struct C2BaseTuning {
    // latency characteristics
    uint32_t latency;
    bool temporal;               // seems this only makes sense if latency is 1..., so this could be captured as latency = 0
    uint32_t delay;

    uint32_t numInputStreams;    // RW? - or suggestion only: RO
    uint32_t numOutputStreams;   // RW
                                 //
    // refs characteristics (per stream?)
    uint32_t maxInputRefs;       // RO
    uint32_t maxOutputRefs;      // RO
    uint32_t maxInputMemory;     // RO - max time refs are held for
    uint32_t maxOutputMemory;    // RO

    // per stream
    bool compressed;
    // format... video/compressed/audio/other?
    // actual "audio/video" format type
    uint32_t width/height? is this needed, or just queue...
    // mime...
};
*/






// overall component
//   => C: domain: audio or video
//   => C: kind: decoder, encoder or filter
//   => "mime" class

//   => C: temporal (bool) => does this depend on ordering?
//   => I: latency
//   => I: history max duration...
//   => I: history max frames kept...
//   => I: reordering depth
//   => I: frc (bool) (perhaps ratio?)
//   => I: current frc

//   - pause
//   => last frame 'number' processed
//   => current frame 'number' processed
//   => invalid settings =>[]

// video decoder configuration:                                 // audio
//   - encoding                                                 // -encoding
//   - hint: max width/height                                   // -hint: sample rate, channels
//   - hint: profile/level                                      // -hint: tools used
//   - hint: framerate (bitrate?)                               // -hint: bitrate
//   - default: color space (from container)
//   - hint: color format                                       // -hint: pcm-encoding
//   - hint: # of views (e.g. MVC)                              // -hint?: channel groups
//   - default: HDR static info (from container)                // -hint?: channel mappings
//   - hint: rotation (e.g. for allocator)

// => # of streams required and their formats? (setting?)
// => # of streams produced and their formats? (tuning)

// => output
//   - # of views                                               // -channel groups && channel mappings
//   - width/height/crop/color format/color space/HDR static info (from buffers)
//     (as required by the allocator & framework)
//   - SEI (or equivalent) <= [port]
//     - CC
//   - reference info

// video encoder configurations
//   - encoding                                                 // - encoding
//   - hint: width/height                                       // - hint: sample rate, channels
//   - hint: frame rate
//   - hint: max width/height (? does this differ from width/height?)
//   - # of input (e.g. MVC)                                    // - hint: # groups and mappings
//   - # of output (e.g. SVC) => bitrates/width/height/framerates? per stream
//   - hint: profile/level                                      // - hint: profile/level
//   - HDR static info + (info: HDR)
//   - color space
//   - hint: color format?                                      // - hint: pcm encoding
//   - SEI
//     - CC
//   - reference directive
//   - hint: bitrate (or quality)                               // - hint: bitrate/quality
//   - optional: codec-specific parameters                      // - optional: csd

// => output                                                    // => output
//   - layers per stream?                                       // E-AC3?... DTS?...Dolby-Vision?
//   - reference info


// RM:
//   - need SPS for full knowledge => component should return max. (component can use less)
//   - critical parameters? (interlaced? profile? level?)

struct C2VideoSizeStruct {
    inline C2VideoSizeStruct() = default;
    inline C2VideoSizeStruct(int32_t width_, int32_t height_)
        : width(width_), height(height_) {}

    int32_t width;     ///< video width
    int32_t height;    ///< video height

    DEFINE_AND_DESCRIBE_BASE_C2STRUCT(VideoSize)
    C2FIELD(width, "width")
    C2FIELD(height, "height")
};

// video size for video decoder [OUT]
typedef C2StreamParam<C2Info, C2VideoSizeStruct, kParamIndexVideoSize> C2VideoSizeStreamInfo;
constexpr char C2_NAME_STREAM_VIDEO_SIZE_INFO[] = "raw.size";

// max video size for video decoder [IN]
typedef C2PortParam<C2Setting, C2VideoSizeStruct, kParamIndexMaxVideoSizeHint> C2MaxVideoSizeHintPortSetting;

// video encoder size [IN]
typedef C2StreamParam<C2Tuning, C2VideoSizeStruct, kParamIndexVideoSizeTuning> C2VideoSizeStreamTuning;
constexpr char C2_NAME_STREAM_VIDEO_SIZE_SETTING[] = "raw.size";

// video encoder frame rate [IN]
typedef C2StreamParam<C2Info, C2FloatValue, kParamIndexFrameRate> C2StreamFrameRateInfo;
constexpr char C2_NAME_STREAM_FRAME_RATE_SETTING[] = "coded.frame-rate";

/// @}

#endif  // C2CONFIG_H_
