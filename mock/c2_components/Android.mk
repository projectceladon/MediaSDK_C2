LOCAL_PATH:= $(call my-dir)

include $(MFX_C2_HOME)/mfx_c2_env.mk

include $(CLEAR_VARS)
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := \
    src/mfx_c2_mock_component.cpp \
    ../../c2_components/src/mfx_c2_component.cpp \
    ../../c2_components/src/mfx_c2_components_registry.cpp

LOCAL_C_INCLUDES := \
    $(MFX_C2_HOME)/c2_components/include/ \
    $(MFX_C2_HOME)/c2_utils/include/ \
    $(MFX_C2_INCLUDES)

LOCAL_CFLAGS := \
    $(MFX_C2_CFLAGS) \
    -DMOCK_COMPONENTS

LOCAL_LDFLAGS := $(MFX_C2_LDFLAGS)

LOCAL_SHARED_LIBRARIES := \
    libhardware libcutils\
    libdl liblog \
    $(MFX_C2_SHARED_LIBS)

LOCAL_STATIC_LIBRARIES := \
    libmfx_c2_utils

LOCAL_HEADER_LIBRARIES := $(MFX_C2_HEADER_LIBRARIES)

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := libmfx_mock_c2_components

include $(BUILD_SHARED_LIBRARY)
