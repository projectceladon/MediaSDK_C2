LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk


LOCAL_SRC_FILES := \
    src/mfx_c2_mock_component.cpp \
    ../../c2_components/src/mfx_c2_component.cpp \
    ../../c2_components/src/mfx_c2_components_registry.cpp

MFX_C2_HOME := $(MFX_HOME)/samples/sample_c2_plugins/
MFX_C_INCLUDES_C2 := $(MFX_C2_HOME)/codec2/include/

LOCAL_C_INCLUDES += \
    $(MFX_C2_HOME)/c2_components/include/ \
    $(MFX_C2_HOME)/c2_utils/include/ \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14 \
    -DMOCK_COMPONENTS

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_SHARED_LIBRARIES := \
  libdl liblog

LOCAL_STATIC_LIBRARIES += libmfx_c2_utils libmfx_mock_codec2

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_mock_c2_components

include $(BUILD_SHARED_LIBRARY)
