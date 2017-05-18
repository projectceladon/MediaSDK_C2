LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

MFX_C_INCLUDES_C2 := $(LOCAL_PATH)/../codec2/include/

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_LIBVA) \
    $(MFX_C_INCLUDES_C2)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_c2_utils

include $(BUILD_STATIC_LIBRARY)
