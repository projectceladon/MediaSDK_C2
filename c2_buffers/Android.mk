LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES += \
    $(MFX_INCLUDES) \
    $(MFX_INCLUDES_C2) \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_C2)

LOCAL_HEADER_LIBRARIES := \
    $(MFX_HEADER_LIBRARIES) \
    libhardware_headers       # It's here due to <hardware/gralloc.h> include. Need to remove when the header will be removed

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_c2_buffers

include $(BUILD_STATIC_LIBRARY)
