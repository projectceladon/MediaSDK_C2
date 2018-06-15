LOCAL_PATH:= $(call my-dir)

include $(MFX_C2_HOME)/mfx_c2_env.mk

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_C2_HOME)/mfx_c2_defs.mk

LOCAL_SRC_FILES := \
    src/c2_store_test.cpp \
    src/test_components.cpp \
    src/test_main.cpp

LOCAL_C_INCLUDES := \
    $(MFX_C2_INCLUDES) \
    $(MFX_C2_HOME)/c2_store/include \
    $(MFX_C2_HOME)/c2_components/include \
    $(MFX_C2_HOME)/unittests/include \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_CFLAGS := $(MFX_C2_CFLAGS)

LOCAL_LDFLAGS := $(MFX_C2_LDFLAGS)

LOCAL_STATIC_LIBRARIES := libgtest libz
LOCAL_SHARED_LIBRARIES := libdl liblog libmfx_c2_store
LOCAL_HEADER_LIBRARIES := $(MFX_C2_HEADER_LIBRARIES)

LOCAL_MULTILIB := both
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE := mfx_c2_store_unittests
LOCAL_MODULE_STEM_32 := mfx_c2_store_unittests32
LOCAL_MODULE_STEM_64 := mfx_c2_store_unittests64

include $(BUILD_EXECUTABLE)

# =============================================================================

include $(CLEAR_VARS)
include $(MFX_C2_HOME)/mfx_c2_defs.mk

STREAM_CPP_FILES := $(wildcard $(LOCAL_PATH)/streams/*/*.cpp)

LOCAL_SRC_FILES := \
    $(STREAM_CPP_FILES:$(LOCAL_PATH)/%=%) \
    src/c2_decoder_test.cpp \
    src/c2_encoder_test.cpp \
    src/test_components.cpp \
    src/test_streams.cpp \
    src/test_main.cpp

LOCAL_C_INCLUDES := \
    $(MFX_C2_INCLUDES) \
    $(MFX_C2_HOME)/c2_components/include \
    $(MFX_C2_HOME)/c2_streams/include \
    $(MFX_C2_HOME)/unittests/include \
    $(MFX_C2_HOME)/c2_utils/include

LOCAL_CFLAGS := $(MFX_C2_CFLAGS)

LOCAL_LDFLAGS := $(MFX_C2_LDFLAGS)

LOCAL_STATIC_LIBRARIES := \
    libmfx_c2_utils \
    libgtest libz \
    $(MFX_C2_STATIC_LIBS)

LOCAL_SHARED_LIBRARIES := \
    libdl \
    liblog \
    libmfx_c2_components_hw \
    $(MFX_C2_SHARED_LIBS)

LOCAL_HEADER_LIBRARIES := $(MFX_C2_HEADER_LIBRARIES)

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
  include $(MFX_C2_HOME)/mfx_c2_defs.mk

  STREAM_CPP_FILES := $$(wildcard $(LOCAL_PATH)/streams/*/*.cpp)

  LOCAL_SRC_FILES := \
      $$(STREAM_CPP_FILES:$(LOCAL_PATH)/%=%) \
      src/c2_mock_component_test.cpp \
      src/c2_utils_test.cpp \
      src/c2_vndk_test.cpp \
      src/test_components.cpp \
      src/test_streams.cpp \
      src/test_frame_constructor.cpp \
      src/test_main.cpp

  LOCAL_C_INCLUDES := \
      $$(MFX_C2_INCLUDES) \
      $$(MFX_C2_HOME)/mock/c2_components/include \
      $$(MFX_C2_HOME)/c2_components/include \
      $$(MFX_C2_HOME)/unittests/include \
      $$(MFX_C2_HOME)/c2_utils/include

  LOCAL_CFLAGS := $$(MFX_C2_CFLAGS)

  ifeq ($(USE_MOCK_CODEC2),true)
      LOCAL_CFLAGS += -DUSE_MOCK_CODEC2
  endif

  LOCAL_LDFLAGS := $$(MFX_C2_LDFLAGS)

  LOCAL_STATIC_LIBRARIES := \
    libgtest libz \
    $(MFX_C2_STATIC_LIBS)

  LOCAL_SHARED_LIBRARIES := \
    libmfx_mock_c2_components \
    libdl \
    liblog \
    libhardware \
    $(MFX_C2_SHARED_LIBS)

  ifneq ($(1),pure)
    MODULE_SUFFIX :=

    LOCAL_C_INCLUDES += $$(MFX_C2_INCLUDES_LIBVA)
    LOCAL_CFLAGS += $$(MFX_C2_CFLAGS_LIBVA)
    LOCAL_SHARED_LIBRARIES += libva libva-android
    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils_va
  else
    MODULE_SUFFIX := _pure

    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils
  endif

  LOCAL_HEADER_LIBRARIES := $$(MFX_C2_HEADER_LIBRARIES)

  LOCAL_MULTILIB := both
  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := mfx_c2_mock_unittests$$(MODULE_SUFFIX)
  LOCAL_MODULE_STEM_32 := mfx_c2_mock_unittests$$(MODULE_SUFFIX)32
  LOCAL_MODULE_STEM_64 := mfx_c2_mock_unittests$$(MODULE_SUFFIX)64

  include $(BUILD_EXECUTABLE)

endef

$(eval $(call build_mock_unittests,va)) # utils test with va

$(eval $(call build_mock_unittests,pure)) # utils test without va
