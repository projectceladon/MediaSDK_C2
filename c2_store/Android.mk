LOCAL_PATH:= $(call my-dir)

include $(MFX_C2_HOME)/mfx_c2_env.mk

include $(CLEAR_VARS)
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

LOCAL_C_INCLUDES := \
    $(MFX_C2_INCLUDES) \
    $(MFX_C2_HOME)/c2_utils/include \
    $(MFX_C2_HOME)/c2_components/include

LOCAL_CFLAGS := $(MFX_C2_CFLAGS)

LOCAL_LDFLAGS := $(MFX_C2_LDFLAGS)

LOCAL_SHARED_LIBRARIES := $(MFX_C2_SHARED_LIBS) libdl liblog
LOCAL_STATIC_LIBRARIES := $(MFX_C2_STATIC_LIBS) libmfx_c2_utils
LOCAL_HEADER_LIBRARIES := $(MFX_C2_HEADER_LIBRARIES)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_c2_store

include $(BUILD_SHARED_LIBRARY)
