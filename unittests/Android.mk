LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

MFX_C_INCLUDES_C2 := $(LOCAL_PATH)/../codec2/include/

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_HOME)/samples/sample_c2_plugins/c2_store/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_SHARED_LIBRARIES := libdl liblog libmfx_c2_store

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_unittests

include $(BUILD_EXECUTABLE)
