LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, c2_store_test.cpp test_utils.cpp)

MFX_C2_HOME := $(MFX_HOME)/samples/sample_c2_plugins/

MFX_C_INCLUDES_C2 := $(LOCAL_PATH)/../codec2/include/

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_C2_HOME)/c2_store/include \
    $(MFX_C2_HOME)/c2_components/include \
    $(MFX_C2_HOME)/unittests/include \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14 \
    -fexceptions

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_SHARED_LIBRARIES := libdl liblog libmfx_c2_store

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_store_unittests

include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := $(addprefix src/, c2_mock_component_test.cpp test_utils.cpp)

LOCAL_C_INCLUDES := \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_C2_HOME)/mock/c2_components/include \
    $(MFX_C2_HOME)/c2_components/include \
    $(MFX_C2_HOME)/unittests/include \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14 \
    -fexceptions

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_SHARED_LIBRARIES := libdl liblog libmfx_c2_components

LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_components_unittests

include $(BUILD_EXECUTABLE)
