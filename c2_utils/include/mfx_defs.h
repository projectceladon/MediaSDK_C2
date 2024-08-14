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

#include <mfxvideo++.h>
#include <mfxvp8.h>
#include <limits>
#include <algorithm>
#include <list>
#include <mfx_android_config.h>

#if (MFX_VERSION >= 2000)
    #include "mfxdispatcher.h"
    #define USE_ONEVPL
#endif

// includes below are to get Intel color formats

#define HAVE_GRALLOC4 // We use gralloc4 but keep supporting gralloc1

#ifdef HAVE_GRALLOC4
    #define USE_GRALLOC4
#else // HAVE_GRALLOC4
#ifdef MFX_C2_USE_PRIME
    // USE_GRALLOC1 required for using PRIME buffer descriptor -
    // opens definition GRALLOC1_PFN_GET_PRIME in
    // i915_private_android_types.h
    #define USE_GRALLOC1
#endif // MFX_C2_USE_PRIME
#endif // HAVE_GRALLOC4
#define DRV_I915
#include <i915_private_android_types.h>

#define MFX_IMPLEMENTATION (MFX_IMPL_AUTO_ANY | MFX_IMPL_VIA_ANY)

extern mfxVersion g_required_mfx_version;

#ifdef LIBVA_SUPPORT
    #include <va/va.h>
#endif // #ifdef LIBVA_SUPPORT

constexpr uint32_t DEFAULT_MAX_INSTANCES = 32;

#define MFX_MAX_PATH 260

#define WIDTH_16K 16384
#define HEIGHT_16K 16384

#define WIDTH_8K 8192
#define HEIGHT_8K 8192

#define WIDTH_4K 4096
#define HEIGHT_4K 4096

#define WIDTH_2K 2048
#define HEIGHT_2K 2048

#define WIDTH_1K 1024
#define HEIGHT_1K 1024

#define MIN_WIDTH_4K 3840
#define MIN_HEIGHT_4K 2160
#define MIN_WIDTH_8K 7680
#define MIN_HEIGHT_8K 4320

#define IS_4K_VIDEO(W, H) (((W) >= MIN_WIDTH_4K && (W) < MIN_WIDTH_8K) || ((H) >= MIN_HEIGHT_4K && (H) < MIN_HEIGHT_8K))
#define IS_8K_VIDEO(W, H) (((W) >= MIN_WIDTH_8K) || ((H) >= MIN_HEIGHT_8K))

#define MFX_TIMEOUT_INFINITE 0xEFFFFFFF

#define MFX_MEM_ALIGN(X, N) ((X) & ((N)-1)) ? (((X)+(N)-1) & (~((N)-1))): (X)

#define MFX_ALIGN_16(X) ((((X) + 15) >> 4) << 4)
#define MFX_ALIGN_32(X) ((((X) + 31) >> 5) << 5)

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

// 16-byte align requirement
#define MFX_C2_IS_COPY_NEEDED(_mem_type, _input_info, _mfx_info)\
    ((MFX_MEMTYPE_SYSTEM_MEMORY == _mem_type) && \
    (((_input_info).Width < (_mfx_info.Width)) || \
    ((_input_info).Height < (_mfx_info.Height))))

#define MFX_TRY_AND_CATCH(_try, _catch) \
    { try { _try; } catch(...) { _catch; } }

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

mfxStatus InitMfxFrameSW(
    uint64_t timestamp, uint64_t frame_index,
    uint8_t *data_Y, uint8_t *data_UV,
    uint32_t width, uint32_t height, uint32_t stride, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame);

mfxStatus InitMfxFrameHW(
    uint64_t timestamp, uint64_t frame_index,
    mfxMemId mem_id,
    uint32_t width, uint32_t height, uint32_t fourcc, const mfxFrameInfo& info, mfxFrameSurface1* mfx_frame);

mfxStatus MFXLoadSurfaceSW(uint8_t *data_Y, uint8_t *data_UV, uint32_t stride, const mfxFrameInfo& input_info, mfxFrameSurface1* srf);

uint32_t MFXGetSurfaceSize(uint32_t FourCC, uint32_t width, uint32_t height);
uint32_t MFXGetFreeSurfaceIdx(mfxFrameSurface1 *SurfacesPool, uint32_t nPoolSize);

mfxStatus MFXAllocSystemMemorySurfacePool(uint8_t **buf, mfxFrameSurface1 *surfpool, mfxFrameInfo frame_info, uint32_t surfnum);
void MFXFreeSystemMemorySurfacePool(uint8_t *buf, mfxFrameSurface1 *surfpool);

uint32_t MFXGetSurfaceWidth(mfxFrameInfo info, bool using_video_memory = true);
uint32_t MFXGetSurfaceHeight(mfxFrameInfo info, bool using_video_memory = true);
