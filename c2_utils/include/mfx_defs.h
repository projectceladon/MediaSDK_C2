/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2021 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <mfxvideo++.h>
#include <mfxvp8.h>
#include <limits>
#include <algorithm>
#include <list>

// includes below are to get Intel color formats
#ifdef MFX_C2_USE_PRIME
    // USE_GRALLOC1 required for using PRIME buffer descriptor -
    // opens definition GRALLOC1_PFN_GET_PRIME in
    // i915_private_android_types.h
    #define USE_GRALLOC1
#endif
#define DRV_I915
#include <i915_private_android_types.h>

#define MFX_IMPLEMENTATION (MFX_IMPL_AUTO_ANY | MFX_IMPL_VIA_ANY)

extern mfxVersion g_required_mfx_version;

#ifdef LIBVA_SUPPORT
    #include <va/va.h>
#endif // #ifdef LIBVA_SUPPORT

#define MFX_MAX_PATH 260

#define MFX_TIMEOUT_INFINITE 0xEFFFFFFF

#define MFX_MEM_ALIGN(X, N) ((X) & ((N)-1)) ? (((X)+(N)-1) & (~((N)-1))): (X)

#define MFX_GET_ARRAY_SIZE(_array) \
    (sizeof(_array) / sizeof(_array[0]))

#define MFX_NEW(_ptr, _class) \
    { try { (_ptr) = new _class; } catch(...) { (_ptr) = NULL; } }

#define MFX_CLASS_NO_COPY(class_name) \
    class_name(const class_name&) = delete; \
    class_name& operator=(const class_name&) = delete;

#define MFX_NEW_NO_THROW(_class) \
    (new (std::nothrow)_class)

#define MFX_DELETE(_ptr) \
    do { delete (_ptr); (_ptr) = nullptr; } while(false)

#define MFX_FREE(_ptr) \
    do { free(_ptr); (_ptr) = nullptr; } while(false)

#define MFX_ZERO_MEMORY(_obj) \
    { memset(&(_obj), 0, sizeof(_obj)); }

#define MFX_MAX(A, B) (((A) > (B)) ? (A) : (B))

#define MFX_MIN(A, B) (((A) < (B)) ? (A) : (B))

#define EXPORT __attribute__((visibility("default")))

#define MFX_HIDE __attribute__((visibility("hidden")))

#define NAMED(value) #value << ": " << (value) << "; "

inline uint32_t MakeUint32(uint16_t high, uint16_t low)
{
    return (uint32_t)high << 16 | low;
}

uint32_t inline EstimatedEncodedFrameLen(uint32_t width, uint32_t height)
{
    return ((uint32_t)width * height * 400) / (16 * 16);
}

// The purpose of this template function is cast from wide range integer
// to narrow range integer. Like int32_t -> uint8_t.
// There is no overflow effects, when the value doesn't fit DstType range,
// it gets DstValue min/max accordingly.
template<typename DstType, typename SrcType>
DstType ClampCast(const SrcType& v)
{
    const DstType lo = std::numeric_limits<DstType>::min();
    const DstType hi = std::numeric_limits<DstType>::max();

    return static_cast<DstType>( (v < lo) ? lo : ((v > hi) ? hi : v) );
}

// Searches array of std::pairs on pair::first field, returns pair::second.
template<typename First, typename Second, int N, typename Result>
bool FirstToSecond(const std::pair<First, Second>(& array)[N], First key, Result* value)
{
    bool res = false;

    auto it = std::find_if(std::begin(array), std::end(array),
        [key] (const auto& pair) -> bool { return pair.first == key; } );

    if(it != std::end(array)) {
        res = true;
        *value = static_cast<Result>(it->second);
    }
    return res;
}

// Searches array of std::pairs on pair::second field, returns pair::first.
template<typename First, typename Second, int N, typename Result>
bool SecondToFirst(const std::pair<First, Second>(& array)[N], Second key, Result* value)
{
    bool res = false;

    auto it = std::find_if(std::begin(array), std::end(array),
        [key] (const auto& pair) -> bool { return pair.second == key; } );

    if(it != std::end(array)) {
        res = true;
        *value = static_cast<Result>(it->first);
    }
    return res;
}

template<typename T>
std::vector<T> MakeVector(T&& item)
{
    std::vector<T> res;
    res.push_back(std::move(item));
    return res;
}

void InitMfxNV12FrameSW(
    uint64_t timestamp, uint64_t frame_index,
    const uint8_t *const *data,
    uint32_t width, uint32_t height, uint32_t stride, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame);

void InitMfxFrameHW(
    uint64_t timestamp, uint64_t frame_index,
    mfxMemId mem_id,
    uint32_t width, uint32_t height, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame);
