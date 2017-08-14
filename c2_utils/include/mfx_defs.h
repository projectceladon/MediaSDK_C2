/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#ifndef __MFX_DEFS_H__
#define __MFX_DEFS_H__

#include <mfxvideo++.h>
#include <mfxvp8.h>
#include <limits>

#define MFX_IMPLEMENTATION (MFX_IMPL_AUTO_ANY | MFX_IMPL_VIA_ANY)

extern mfxVersion g_required_mfx_version;

#ifdef LIBVA_SUPPORT
    #include <va/va.h>
    #include <va/va_tpi.h>
#endif // #ifdef LIBVA_SUPPORT

#define MFX_MAX_PATH 260

#define MFX_C2_INFINITE 0xEFFFFFFF

#define MFX_GET_ARRAY_SIZE(_array) \
    (sizeof(_array) / sizeof(_array[0]))

#define MFX_CLASS_NO_COPY(class_name) \
    class_name(const class_name&) = delete; \
    class_name& operator=(const class_name&) = delete;

#define MFX_NEW_NO_THROW(_class) \
    (new (std::nothrow)_class)

#define MFX_DELETE(_ptr) \
    do { delete (_ptr); (_ptr) = NULL; } while(false)

#define MFX_FREE(_ptr) \
    do { free(_ptr); (_ptr) = NULL; } while(false)

#define MFX_ZERO_MEMORY(_obj) \
    { memset(&(_obj), 0, sizeof(_obj)); }

#define EXPORT __attribute__((visibility("default")))

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

#endif // #ifndef __MFX_DEFS_H__
