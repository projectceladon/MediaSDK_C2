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

#ifndef C2WORK_H_

#define C2WORK_H_

#include <stdint.h>
#include <stdbool.h>
#include <C2Param.h>
#include <C2Buffer.h>
#include <C2Config.h>

#include <memory>
#include <list>
#include <vector>

typedef int status_t;

namespace android {

/// \defgroup work Work and data processing
/// @{

struct C2SettingResult {
    enum Failure {
        READ_ONLY,  ///< parameter is read-only and cannot be set
        MISMATCH,   ///< parameter mismatches input data
        BAD_VALUE,  ///< parameter does not accept value
        BAD_TYPE,   ///< parameter is not supported
        BAD_PORT,   ///< parameter is not supported on the specific port
        BAD_INDEX,  ///< parameter is not supported on the specific stream
        CONFLICT,   ///< parameter is in conflict with another setting
    };

    Failure failure;    ///< failure code

    /// Failing (or corrected) field. Currently supported values for the field. This is set if
    /// different from the globally supported values (e.g. due to restrictions by another param or
    /// input data)
    /// \todo need to define suggestions for masks to be set and unset.
    C2ParamFieldValues field;

    /// Conflicting parameters or fields with optional suggestions with (optional) suggested values
    /// for any conflicting fields to avoid the conflict.
    std::list<C2ParamFieldValues> conflicts;
};

// ================================================================================================
//  WORK
// ================================================================================================

// c2_node_id_t-s
typedef uint32_t c2_node_id_t;

enum {
    kParamIndexWorkOrdinal,
};

struct C2WorkOrdinalStruct {
//public:
    c2_cntr64_t timestamp;     /** frame timestamp in microseconds */
    c2_cntr64_t frameIndex;    /** submission ordinal on the initial component */
    c2_cntr64_t customOrdinal; /** can be given by the component, e.g. decode order */

    DEFINE_AND_DESCRIBE_C2STRUCT(WorkOrdinal)
    C2FIELD(timestamp, "timestamp")
    C2FIELD(frameIndex, "frame-index")
    C2FIELD(customOrdinal, "custom-ordinal")
};

struct C2FrameData {
//public:
    enum flags_t : uint32_t {
        /**
         * For input frames: no output frame shall be generated when processing this frame, but
         * metadata shall still be processed.
         * For output frames: this frame shall be discarded and but metadata is still valid.
         */
        FLAG_DROP_FRAME    = (1 << 0),
        /**
         * This frame is the last frame of the current stream. Further frames are part of a new
         * stream.
         */
        FLAG_END_OF_STREAM = (1 << 1),
        /**
         * This frame shall be discarded with its metadata.
         * This flag is only set by components - e.g. as a response to the flush command.
         */
        FLAG_DISCARD_FRAME = (1 << 2),
        /**
         * This frame contains only codec-specific configuration data, and no actual access unit.
         *
         * \deprecated pass codec configuration with using the \todo codec-specific configuration
         * info together with the access unit.
         */
        FLAG_CODEC_CONFIG  = (1u << 31),
    };

    /**
     * Frame flags */
    flags_t  flags;
    C2WorkOrdinalStruct ordinal;
    std::vector<std::shared_ptr<C2Buffer>> buffers;
    //< for initial work item, these may also come from the parser - if provided
    //< for output buffers, these are the responses to requestedInfos
    std::list<std::unique_ptr<C2Info>>       infos;
    std::list<std::shared_ptr<C2InfoBuffer>> infoBuffers;
};

struct C2Worklet {
//public:
    // IN
    c2_node_id_t component;

    /** Configuration changes to be applied before processing this worklet. */
    std::vector<std::unique_ptr<C2Tuning>> tunings;
    std::list<C2Param::Type> requestedInfos;

    // OUT
    C2FrameData output;
    std::list<std::unique_ptr<C2SettingResult>> failures;
};

/**
 * This structure holds information about all a single work item.
 *
 * This structure shall be passed by the client to the component for the first worklet. As such,
 * worklets must not be empty. The ownership of this object is passed.
 *
 * input:
 *      The input data to be processed. This is provided by the client with ownership. When the work
 *      is returned, the input buffer-pack's buffer vector shall contain nullptrs.
 *
 * worklets:
 *      The chain of components and associated allocators, tunings and info requests that the data
 *      must pass through. If this has more than a single element, the tunnels between successive
 *      components of the worklet chain must have been (successfully) pre-registered at the time
 *      the work is submitted. Allocating the output buffers in the worklets is the responsibility
 *      of each component. Upon work submission, each output buffer-pack shall be an appropriately
 *      sized vector containing nullptrs. When the work is completed/returned to the client,
 *
 * worklets_processed:
 *      It shall be initialized to 0 by the client when the work is submitted.
 *      It shall contain the number of worklets that were successfully processed when the work is
 *      returned. If this is less then the number of worklets, result must not be success.
 *      It must be in the range of [0, worklets.size()].
 *
 * result:
 *      The final outcome of the work. If 0 when work is returned, it is assumed that all worklets
 *      have been processed.
 */
struct C2Work {
//public:
    // pre-chain infos (for portions of a tunneling chain that happend before this work-chain for
    // this work item - due to framework facilitated (non-tunneled) work-chaining)
    std::list<std::pair<std::unique_ptr<C2PortMimeConfig>, std::unique_ptr<C2Info>>> preChainInfos;
    std::list<std::pair<std::unique_ptr<C2PortMimeConfig>, std::unique_ptr<C2Buffer>>> preChainInfoBlobs;

    C2FrameData input;
    std::list<std::unique_ptr<C2Worklet>> worklets;

    uint32_t workletsProcessed;
    c2_status_t result;
};

struct C2WorkOutline {
//public:
    C2WorkOrdinalStruct ordinal;
    std::list<c2_node_id_t> chain;
};

/// @}

}  // namespace android

#endif  // C2WORK_H_
