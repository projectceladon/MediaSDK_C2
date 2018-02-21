LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := \
    src/c2_store_test.cpp \
    src/gtest_emulation.cpp \
    src/test_components.cpp \
    src/test_main.cpp

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
    $(MFX_CFLAGS_C2)

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_STATIC_LIBRARIES := libippdc_l libippcore_l

LOCAL_SHARED_LIBRARIES := libdl liblog libmfx_c2_store

LOCAL_HEADER_LIBRARIES := \
    $(MFX_HEADER_LIBRARIES) \
    libhardware_headers       # It's here due to <hardware/gralloc.h> include. Need to remove when the header will be removed

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_store_unittests
LOCAL_MODULE_STEM_32 := mfx_c2_store_unittests32
LOCAL_MODULE_STEM_64 := mfx_c2_store_unittests64

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_HOME)/android/mfx_defs.mk
include $(MFX_C2_HOME)/mfx_c2_defs.mk

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
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_C_INCLUDES_32 := $(IPP_ROOT_32)/include
LOCAL_C_INCLUDES_64 := $(IPP_ROOT_64)/include

LOCAL_CFLAGS += \
    $(MFX_CFLAGS) \
    $(MFX_CFLAGS_C2)

LOCAL_LDFLAGS += \
    $(MFX_LDFLAGS)

LOCAL_STATIC_LIBRARIES := \
    libmfx_c2_utils \
    libippdc_l \
    libippcore_l \
    $(MFX_STATIC_LIBS_C2)

LOCAL_SHARED_LIBRARIES := \
    libdl \
    liblog \
    libmfx_c2_components_hw \
    $(MFX_SHARED_LIBS_C2)

LOCAL_HEADER_LIBRARIES := \
    $(MFX_HEADER_LIBRARIES) \
    libhardware_headers       # It's here due to <hardware/gralloc.h> include. Need to remove when the header will be removed

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_components_unittests
LOCAL_MODULE_STEM_32 := mfx_c2_components_unittests32
LOCAL_MODULE_STEM_64 := mfx_c2_components_unittests64

include $(BUILD_EXECUTABLE)

# =============================================================================

# Usage: $(call build_mock_unittests, va|pure)
define build_mock_unittests

  include $(CLEAR_VARS)
  include $(MFX_HOME)/android/mfx_defs.mk
  include $(MFX_C2_HOME)/mfx_c2_defs.mk

  STREAM_CPP_FILES := $$(wildcard $(LOCAL_PATH)/streams/*/*.cpp)

  LOCAL_SRC_FILES := \
      $$(STREAM_CPP_FILES:$(LOCAL_PATH)/%=%) \
      src/c2_mock_component_test.cpp \
      src/c2_utils_test.cpp \
      src/gtest_emulation.cpp \
      src/test_components.cpp \
      src/test_streams.cpp \
      src/test_frame_constructor.cpp \
      src/test_main.cpp

  LOCAL_C_INCLUDES := \
      $$(MFX_C_INCLUDES) \
      $$(MFX_C_INCLUDES_C2) \
      $$(MFX_C2_HOME)/mock/c2_components/include \
      $$(MFX_C2_HOME)/c2_components/include \
      $$(MFX_C2_HOME)/unittests/include \
      $$(MFX_C2_HOME)/c2_utils/include

  LOCAL_C_INCLUDES_32 := $$(IPP_ROOT_32)/include
  LOCAL_C_INCLUDES_64 := $$(IPP_ROOT_64)/include

  LOCAL_CFLAGS += \
      $$(MFX_CFLAGS) \
      $$(MFX_CFLAGS_C2)

  LOCAL_LDFLAGS += \
      $$(MFX_LDFLAGS)

  LOCAL_STATIC_LIBRARIES := \
    libippdc_l \
    libippcore_l \
    $(MFX_STATIC_LIBS_C2)

  LOCAL_SHARED_LIBRARIES := \
    libmfx_mock_c2_components \
    libdl \
    liblog \
    libhardware \
    $(MFX_SHARED_LIBS_C2)

  ifneq ($(1),pure)
    MODULE_SUFFIX :=

    LOCAL_C_INCLUDES += \
        $$(MFX_C_INCLUDES_LIBVA)
    LOCAL_CFLAGS += \
        $$(MFX_CFLAGS_LIBVA)
    LOCAL_SHARED_LIBRARIES += \
      libva libva-android

    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils_va
  else
    MODULE_SUFFIX := _pure

    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils
  endif

  LOCAL_HEADER_LIBRARIES := \
      $$(MFX_HEADER_LIBRARIES)

  LOCAL_MULTILIB := both
  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := mfx_c2_mock_unittests$$(MODULE_SUFFIX)
  LOCAL_MODULE_STEM_32 := mfx_c2_mock_unittests$$(MODULE_SUFFIX)32
  LOCAL_MODULE_STEM_64 := mfx_c2_mock_unittests$$(MODULE_SUFFIX)64

  include $(BUILD_EXECUTABLE)

endef

$(eval $(call build_mock_unittests,va)) # utils test with va

$(eval $(call build_mock_unittests,pure)) # utils test without va
