LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_C2_HOME)/c2_utils/include \
    $(LOCAL_PATH)/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_C2)

LOCAL_SHARED_LIBRARIES := \
    libhardware \
    libui

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_mock_codec2

include $(BUILD_STATIC_LIBRARY)
