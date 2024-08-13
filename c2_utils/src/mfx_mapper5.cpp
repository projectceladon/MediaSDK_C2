// Copyright (c) 2024 Intel Corporation
// SPDX-License-Identifier: Apache-2.0

#include <aidl/android/hardware/graphics/allocator/IAllocator.h>
#include <aidl/android/hardware/graphics/common/BufferUsage.h>
#include <aidl/android/hardware/graphics/common/ChromaSiting.h>
#include <aidl/android/hardware/graphics/common/Dataspace.h>
#include <aidl/android/hardware/graphics/common/ExtendableType.h>
#include <aidl/android/hardware/graphics/common/PixelFormat.h>
#include <aidl/android/hardware/graphics/common/PlaneLayoutComponent.h>
#include <aidl/android/hardware/graphics/common/PlaneLayoutComponentType.h>
#include <aidl/android/hardware/graphics/common/StandardMetadataType.h>
#include <android/hardware/graphics/mapper/utils/IMapperMetadataTypes.h>
#include <android/binder_manager.h>
#include <dlfcn.h> // dlopen
#include <gralloctypes/Gralloc4.h>
#include <system/window.h>
#include <ui/FatVector.h>
#include <vndksupport/linker.h>

#include "mfx_mapper5.h"
#include "cros_gralloc/cros_gralloc_helpers.h"
#include "mfx_c2_utils.h"
#include "mfx_debug.h"
#include "mfx_c2_debug.h"

using namespace aidl::android::hardware::graphics::allocator;
using aidl::android::hardware::graphics::common::BufferUsage;
using aidl::android::hardware::graphics::common::ChromaSiting;
using aidl::android::hardware::graphics::common::Dataspace;
using aidl::android::hardware::graphics::common::ExtendableType;
using aidl::android::hardware::graphics::common::PlaneLayout;
using aidl::android::hardware::graphics::common::PlaneLayoutComponent;
using aidl::android::hardware::graphics::common::PlaneLayoutComponentType;
using aidl::android::hardware::graphics::common::PlaneLayout;
using aidl::android::hardware::graphics::common::StandardMetadataType;
using android::hardware::graphics::common::V1_2::PixelFormat;
using android::hardware::graphics::mapper::StandardMetadata;
using android::hardware::hidl_handle;

using ADataspace = aidl::android::hardware::graphics::common::Dataspace;
using APixelFormat = aidl::android::hardware::graphics::common::PixelFormat;

#ifdef USE_MAPPER5

static const auto kIAllocatorServiceName = IAllocator::descriptor + std::string("/default");
static const auto kIAllocatorMinimumVersion = 2;

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_mapper5"


typedef AIMapper_Error (*AIMapper_loadIMapperFn)(AIMapper *_Nullable *_Nonnull outImplementation);

static std::shared_ptr<IAllocator> waitForAllocator() {
    if (__builtin_available(android 31, *)) {
        if (!AServiceManager_isDeclared(kIAllocatorServiceName.c_str())) {
            return nullptr;
        }
        auto allocator = IAllocator::fromBinder(
                ndk::SpAIBinder(AServiceManager_waitForService(kIAllocatorServiceName.c_str())));
        if (!allocator) {
            ALOGE("AIDL IAllocator declared but failed to get service");
            return nullptr;
        }

        int32_t version = 0;
        if (!allocator->getInterfaceVersion(&version).isOk()) {
            ALOGE("Failed to query interface version");
            return nullptr;
        }
        if (version < kIAllocatorMinimumVersion) {
            return nullptr;
        }
        return allocator;
    } else {
        return nullptr;
    }
}

static void *loadIMapperLibrary() {
    static void *imapperLibrary = []() -> void * {
        auto allocator = waitForAllocator();
        std::string mapperSuffix;
        auto status = allocator->getIMapperLibrarySuffix(&mapperSuffix);
        if (!status.isOk()) {
            ALOGE("Failed to get IMapper library suffix");
            return nullptr;
        }
        std::string lib_name = "mapper." + mapperSuffix + ".so";
        void *so = android_load_sphal_library(lib_name.c_str(), RTLD_LOCAL | RTLD_NOW);
        if (!so) {
            ALOGE("Failed to load mapper.minigbm.so");
        }
        return so;
    }();
    return imapperLibrary;
}

template <StandardMetadataType T>
static auto getStandardMetadata(AIMapper *mapper, buffer_handle_t bufferHandle)
        -> decltype(StandardMetadata<T>::value::decode(NULL, 0)) {
    using Value = typename StandardMetadata<T>::value;
    // TODO: Tune for common-case better
    android::FatVector<uint8_t, 128> buffer;
    int32_t sizeRequired = mapper->v5.getStandardMetadata(bufferHandle,
                                                          static_cast<int64_t>(T),
                                                          buffer.data(), buffer.size());
    if (sizeRequired < 0) {
        ALOGW_IF(-AIMAPPER_ERROR_UNSUPPORTED != sizeRequired,
                 "Unexpected error %d from valid getStandardMetadata call", -sizeRequired);
        return std::nullopt;
    }
    if ((size_t)sizeRequired > buffer.size()) {
        buffer.resize(sizeRequired);
        sizeRequired = mapper->v5.getStandardMetadata(bufferHandle, static_cast<int64_t>(T),
                                                      buffer.data(), buffer.size());
    }
    if (sizeRequired < 0 || (size_t)sizeRequired > buffer.size()) {
        ALOGW("getStandardMetadata failed, received %d with buffer size %zd", sizeRequired,
              buffer.size());
        // Generate a fail type
        return std::nullopt;
    }
    return Value::decode(buffer.data(), sizeRequired);
}

c2_status_t MfxMapper5Module::Init()
{
    MFX_DEBUG_TRACE_FUNC;

    void *so = loadIMapperLibrary();
    if (!so) {
        ALOGE("loadIMapperLibrary failed");
        return C2_CORRUPTED;
    }
    auto loadIMapper = (AIMapper_loadIMapperFn)dlsym(so, "AIMapper_loadIMapper");
    AIMapper *mapper = nullptr;
    AIMapper_Error error = loadIMapper(&mapper);
    if (error != AIMAPPER_ERROR_NONE) {
        ALOGE("AIMapper_loadIMapper failed %d", error);
        return C2_CORRUPTED;
    }

    m_mapper = mapper;

    return C2_OK;
}

MfxMapper5Module::~MfxMapper5Module()
{
}

c2_status_t MfxMapper5Module::GetBufferDetails(const buffer_handle_t handle, BufferDetails *details)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    if (nullptr == m_mapper) {
        ALOGE("mapper is null");
        return C2_NO_INIT;
    }

    auto importedHandle = ImportBuffer(handle);
    if (nullptr == importedHandle) {
        ALOGE("ImportBuffer failed");
        return C2_CORRUPTED;
    }

    do
    {
        details->handle = handle;

        details->prime = handle->data[0];
        MFX_DEBUG_TRACE_I32(details->prime);

        auto width =
            getStandardMetadata<StandardMetadataType::WIDTH>(m_mapper, importedHandle);
        if (!width.has_value()) {
            ALOGE("WIDTH get failed");
            res = C2_CORRUPTED;
            break;
        }

        details->width = details->allocWidth = *width;
        MFX_DEBUG_TRACE_I32(details->width);

        auto height =
            getStandardMetadata<StandardMetadataType::HEIGHT>(m_mapper, importedHandle);
        if (!height.has_value()) {
            ALOGE("HEIGHT get failed");
            res = C2_CORRUPTED;
            break;
        }
        details->height = details->allocHeight = *height;
        MFX_DEBUG_TRACE_I32(details->height);

        auto pixelFormat =
            getStandardMetadata<StandardMetadataType::PIXEL_FORMAT_REQUESTED>(m_mapper,
                                                                              importedHandle);
        if (!pixelFormat.has_value()) {
            ALOGE("PIXEL_FORMAT_REQUESTED get failed");
            res = C2_CORRUPTED;
            break;
        }

        details->format = static_cast<std::underlying_type_t<PixelFormat>>(*pixelFormat);
        MFX_DEBUG_TRACE_I32(details->format);

        auto layouts_opt = getStandardMetadata<StandardMetadataType::PLANE_LAYOUTS>(m_mapper,
                                                                                    importedHandle);
        if (!(layouts_opt.has_value())) {
            ALOGE("PLANE_LAYOUTS get failed");
            res = C2_CORRUPTED;
            break;
        }

        std::vector<PlaneLayout> &layouts = *layouts_opt;
        details->planes_count = layouts.size();
        MFX_DEBUG_TRACE_I32(details->planes_count);

        for(int i = 0; i < layouts.size(); i++)
        {
            details->pitches[i] = layouts[i].strideInBytes;
            MFX_DEBUG_TRACE_STREAM("details->pitches[" << i << "] = " << details->pitches[i]);
        }
    } while (false);

    (void)FreeBuffer(importedHandle);
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxMapper5Module::GetBackingStore(const buffer_handle_t handle, uint64_t *id)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    auto importedHandle = ImportBuffer(handle);
    if (nullptr == importedHandle) {
        ALOGE("ImportBuffer failed");
        return C2_CORRUPTED;
    }

    auto bufferId =
        getStandardMetadata<StandardMetadataType::BUFFER_ID>(m_mapper, importedHandle);
    if (!bufferId.has_value()) {
        ALOGE("BufferId get failed");
        res = C2_CORRUPTED;
    } else {
        *id = *bufferId;
    }

    (void)FreeBuffer(importedHandle);
    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

buffer_handle_t MfxMapper5Module::ImportBuffer(const buffer_handle_t rawHandle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;
    buffer_handle_t outBuffer = nullptr;

    if (nullptr == m_mapper) {
        ALOGE("mapper is null");
        res = C2_NO_INIT;
    }

    if (C2_OK == res)
    {
        AIMapper_Error error = m_mapper->v5.importBuffer(hardware::hidl_handle(rawHandle),
                                                         &outBuffer);
        if (error != AIMAPPER_ERROR_NONE) {
            ALOGE("importBuffer failed");
            res = C2_CORRUPTED;
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return outBuffer;
}

c2_status_t MfxMapper5Module::FreeBuffer(const buffer_handle_t handle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    if (nullptr == m_mapper) {
        ALOGE("mapper is null");
        res = C2_NO_INIT;
    }

    if (C2_OK == res)
    {
        AIMapper_Error error = m_mapper->v5.freeBuffer(handle);
        if (error != AIMAPPER_ERROR_NONE) {
            ALOGE("freeBuffer failed");
            res = C2_CORRUPTED;
        }
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

c2_status_t MfxMapper5Module::LockFrame(buffer_handle_t handle, uint8_t** data,
                                        C2PlanarLayout *layout)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    if (!layout) {
        ALOGE("layout is invalid");
        return C2_BAD_VALUE;
    }

    if (nullptr == m_mapper) {
        ALOGE("mapper is null");
        return C2_NO_INIT;
    }

    BufferDetails details;

    if (C2_OK != GetBufferDetails(handle, &details)) {
        ALOGE("GetBufferDetails failed");
        return C2_BAD_VALUE;
    }

    const ARect region{0, 0, details.width, details.height};
    uint8_t *img = nullptr;
    AIMapper_Error error = m_mapper->v5.lock(handle,
                    AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_READ_MASK |
                    AHardwareBuffer_UsageFlags::AHARDWAREBUFFER_USAGE_CPU_WRITE_MASK,
                    region, -1, (void**)&img);
    if (error != AIMAPPER_ERROR_NONE) {
        ALOGE("lock failed");
        res = C2_CORRUPTED;
    }

    if (C2_OK == res) {
        InitNV12PlaneLayout(details.pitches, layout);
        InitNV12PlaneData(details.pitches[C2PlanarLayout::PLANE_Y],
                          details.allocHeight, (uint8_t*)img, data);
    }

    return res;
}

c2_status_t MfxMapper5Module::UnlockFrame(buffer_handle_t handle)
{
    MFX_DEBUG_TRACE_FUNC;
    c2_status_t res = C2_OK;

    if (nullptr == m_mapper) {
        ALOGE("mapper is null");
        return C2_NO_INIT;
    }

    int releaseFence = -1;
    AIMapper_Error error = m_mapper->v5.unlock(handle, &releaseFence);
    if (error != AIMAPPER_ERROR_NONE) {
        ALOGE("unlock failed");
        res = C2_CORRUPTED;
    }

    MFX_DEBUG_TRACE__android_c2_status_t(res);
    return res;
}

#endif
