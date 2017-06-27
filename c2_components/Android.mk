LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Usage: $(call build_components, hw|sw|pure)
define build_components
  include $$(CLEAR_VARS)
  include $$(MFX_HOME)/android/mfx_defs.mk

  LOCAL_SRC_FILES := $$(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

  MFX_C2_HOME := $$(MFX_HOME)/samples/sample_c2_plugins/
  MFX_C_INCLUDES_C2 := $$(LOCAL_PATH)/../codec2/include/

  LOCAL_C_INCLUDES := \
      $$(MFX_C_INCLUDES) \
      $$(MFX_C_INCLUDES_C2) \
      $$(MFX_C2_HOME)/c2_utils/include \
      $$(MFX_C2_HOME)/mock/c2_components/include

  LOCAL_CFLAGS := \
      $$(MFX_CFLAGS) \
      -std=c++14

  LOCAL_LDFLAGS += \
      $$(MFX_LDFLAGS)

  LOCAL_SHARED_LIBRARIES := \
    libdl liblog

  LOCAL_STATIC_LIBRARIES += libmfx_c2_utils libmfx_mock_c2_components libmfx_mock_codec2

  ifneq ($(1),pure)
    MODULE_SUFFIX := _$(1)

    MSDK_IMPL := $(1)

    LOCAL_C_INCLUDES += \
        $$(MFX_C_INCLUDES_LIBVA)
    LOCAL_CFLAGS += \
        $$(MFX_CFLAGS_LIBVA)
    LOCAL_SHARED_LIBRARIES += \
      libva libva-android

    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils_va
  else
    MODULE_SUFFIX := _pure

    MSDK_IMPL := sw

    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils
  endif

  LOCAL_SHARED_LIBRARIES_32 := libmfx$$(MSDK_IMPL)32
  LOCAL_SHARED_LIBRARIES_64 := libmfx$$(MSDK_IMPL)64

  LOCAL_MODULE_TAGS := optional

  LOCAL_MODULE := libmfx_c2_components$$(MODULE_SUFFIX)

  include $(BUILD_SHARED_LIBRARY)
endef

ifeq ($(MFX_OMX_IMPL_HW),true)
  $(eval $(call build_components,hw))
endif

ifeq ($(MFX_OMX_IMPL_SW),true)
  $(eval $(call build_components,sw))
endif

$(eval $(call build_components,pure)) # pure components without libVA
