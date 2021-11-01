LOCAL_PATH := $(call my-dir)

include $(MFX_C2_HOME)/mfx_c2_env.mk

include $(CLEAR_VARS)
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_MODULE := libmfx_c2_utils_va

LOCAL_SHARED_LIBRARIES := \
    liblog \
    libexpat \
    libgralloctypes \
    libstagefright_foundation \
    android.hardware.media.bufferpool@2.0 \
    android.hardware.graphics.bufferqueue@2.0 \
    android.hardware.graphics.common@1.2 \
    $(MFX_C2_SHARED_LIBRARIES)

LOCAL_STATIC_LIBRARIES := \
    libexpat

LOCAL_CFLAGS := \
    $(MFX_C2_CFLAGS) \
    $(MFX_C2_CFLAGS_LIBVA) \

LOCAL_HEADER_LIBRARIES := $(MFX_C2_HEADER_LIBRARIES)
LOCAL_HEADER_LIBRARIES += mfx_c2_components_headers

LOCAL_C_INCLUDES := \
    hardware/intel/external/libva \
    frameworks/av/media/codec2/vndk/include \
    frameworks/native/include \
    frameworks/native/libs/ui/include \
    frameworks/native/libs/nativewindow/include \
    frameworks/native/libs/arect/include \
    frameworks/native/libs/nativebase/include \
    frameworks/av/media/codec2/components/base/include \
    frameworks/av/media/codec2/core/include \
    frameworks/av/media/libstagefright/include \
    frameworks/av/media/libstagefright/foundation/include \
    system/core/include/utils \
    system/core/base/include \
    $(MFX_C2_INCLUDES) \
    $(MFX_C2_HOME)/c2_utils/include \
    $(MFX_C2_HOME)/plugin_store/include

LOCAL_EXPORT_C_INCLUDE_DIRS := \
    include

LOCAL_PROPRIETARY_MODULE := true

include $(BUILD_STATIC_LIBRARY)

