LOCAL_PATH:= $(call my-dir)

include $(MFX_C2_HOME)/mfx_c2_env.mk

include $(CLEAR_VARS)
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES := \
    $(MFX_C2_INCLUDES) \
    $(MFX_C2_HOME)/c2_utils/include \
    $(MFX_C2_HOME)/c2_components/include \
    frameworks/av/media/codec2/vndk/include \

LOCAL_CFLAGS := $(MFX_C2_CFLAGS)

LOCAL_HEADER_LIBRARIES := $(MFX_C2_HEADER_LIBRARIES)

LOCAL_SHARED_LIBRARIES += \
        libexpat \
        libgralloctypes \
        libstagefright_foundation \
        android.hardware.media.bufferpool@2.0 \
        android.hardware.graphics.bufferqueue@2.0 \
        android.hardware.graphics.common@1.2

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_c2_buffers

include $(BUILD_STATIC_LIBRARY)
