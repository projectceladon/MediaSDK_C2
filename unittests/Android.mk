LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

LOCAL_SRC_FILES := \
    src/c2_store_test.cpp \
    src/gtest_emulation.cpp \
    src/test_components.cpp \
    src/test_main.cpp

MFX_C2_HOME := $(MFX_HOME)/samples/sample_c2_plugins/

MFX_C_INCLUDES_C2 := $(LOCAL_PATH)/../codec2/include/

LOCAL_C_INCLUDES += \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_C2_HOME)/c2_store/include \
    $(MFX_C2_HOME)/c2_components/include \
    $(MFX_C2_HOME)/unittests/include \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_C_INCLUDES_32 := $(IPP_ROOT_32)/include
LOCAL_C_INCLUDES_64 := $(IPP_ROOT_64)/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14 \
    -fexceptions

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_STATIC_LIBRARIES := libippdc_l libippcore_l

LOCAL_SHARED_LIBRARIES := libdl liblog libmfx_c2_store

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_store_unittests
LOCAL_MODULE_STEM_32 := mfx_c2_store_unittests32
LOCAL_MODULE_STEM_64 := mfx_c2_store_unittests64

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

STREAM_CPP_FILES := $(wildcard $(LOCAL_PATH)/streams/*/*.cpp)

LOCAL_SRC_FILES := \
    $(STREAM_CPP_FILES:$(LOCAL_PATH)/%=%) \
    src/c2_decoder_test.cpp \
    src/c2_encoder_test.cpp \
    src/gtest_emulation.cpp \
    src/test_components.cpp \
    src/test_streams.cpp \
    src/test_main.cpp

LOCAL_C_INCLUDES := \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_C2_HOME)/c2_components/include \
    $(MFX_C2_HOME)/c2_streams/include \
    $(MFX_C2_HOME)/unittests/include \
    $(MFX_C2_HOME)/mock/codec2/include \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_C_INCLUDES_32 := $(IPP_ROOT_32)/include
LOCAL_C_INCLUDES_64 := $(IPP_ROOT_64)/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14 \
    -fexceptions

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_STATIC_LIBRARIES := libmfx_c2_utils libmfx_mock_codec2 libippdc_l libippcore_l

LOCAL_SHARED_LIBRARIES := libdl liblog libmfx_c2_components_hw

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_components_unittests
LOCAL_MODULE_STEM_32 := mfx_c2_components_unittests32
LOCAL_MODULE_STEM_64 := mfx_c2_components_unittests64

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk

STREAM_CPP_FILES := $(wildcard $(LOCAL_PATH)/streams/*/*.cpp)

LOCAL_SRC_FILES := \
    $(STREAM_CPP_FILES:$(LOCAL_PATH)/%=%) \
    src/c2_mock_component_test.cpp \
    src/c2_utils_test.cpp \
    src/gtest_emulation.cpp \
    src/test_components.cpp \
    src/test_frame_constructor.cpp \
    src/test_main.cpp

LOCAL_C_INCLUDES := \
    $(MFX_C_INCLUDES) \
    $(MFX_C_INCLUDES_C2) \
    $(MFX_C2_HOME)/mock/c2_components/include \
    $(MFX_C2_HOME)/c2_components/include \
    $(MFX_C2_HOME)/unittests/include \
    $(MFX_C2_HOME)/mock/codec2/include \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_C_INCLUDES_32 := $(IPP_ROOT_32)/include
LOCAL_C_INCLUDES_64 := $(IPP_ROOT_64)/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    -std=c++14 \
    -fexceptions

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_STATIC_LIBRARIES := libmfx_c2_utils libmfx_mock_codec2 libippdc_l libippcore_l

LOCAL_SHARED_LIBRARIES := libmfx_mock_c2_components \
    libdl liblog libhardware

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_mock_unittests
LOCAL_MODULE_STEM_32 := mfx_c2_mock_unittests32
LOCAL_MODULE_STEM_64 := mfx_c2_mock_unittests64

include $(BUILD_EXECUTABLE)
