LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := \
    src/mfx_c2_mock_component.cpp \
    ../../c2_components/src/mfx_c2_component.cpp \
    ../../c2_components/src/mfx_c2_components_registry.cpp

LOCAL_C_INCLUDES += \
    $(MFX_C2_HOME)/c2_components/include/ \
    $(MFX_C2_HOME)/c2_utils/include/ \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2)

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_C2) \
    -DMOCK_COMPONENTS

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_SHARED_LIBRARIES := \
  libhardware \
  libdl liblog

LOCAL_STATIC_LIBRARIES += libmfx_mock_codec2 libmfx_c2_utils

LOCAL_HEADER_LIBRARIES := \
    $(MFX_HEADER_LIBRARIES)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_mock_c2_components

include $(BUILD_SHARED_LIBRARY)
