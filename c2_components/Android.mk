LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

MFX_C2_HOME := $(MFX_HOME)/samples/sample_c2_plugins/
MFX_C_INCLUDES_C2 := $(LOCAL_PATH)/../codec2/include/

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_C2_HOME)/c2_utils/include \
    $(MFX_C2_HOME)/mock/c2_components/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_SHARED_LIBRARIES := libdl liblog

LOCAL_STATIC_LIBRARIES += libmfx_c2_utils libmfx_mock_c2_components

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_c2_components

include $(BUILD_SHARED_LIBRARY)
