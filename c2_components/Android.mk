LOCAL_PATH:= $(call my-dir)

include $(MFX_C2_HOME)/mfx_c2_env.mk

# Usage: $(call build_components, hw|sw|pure)
define build_components
  include $$(CLEAR_VARS)
  include $$(MFX_C2_HOME)/mfx_c2_defs.mk

  LOCAL_SRC_FILES := $$(addprefix src/, $(notdir $(wildcard $(LOCAL_PATH)/src/*.cpp)))

  LOCAL_C_INCLUDES := \
      $$(MFX_C2_INCLUDES) \
      $$(MFX_C2_HOME)/c2_utils/include \
      $$(MFX_C2_HOME)/c2_buffers/include \
      $$(MFX_C2_HOME)/plugin_store/include \
      frameworks/native/libs/ui/include \
      frameworks/av/media/codec2/sfplugin/utils

  LOCAL_CFLAGS := $$(MFX_C2_CFLAGS)

  LOCAL_LDFLAGS := $$(MFX_C2_LDFLAGS)

  LOCAL_SHARED_LIBRARIES := \
    libhardware libdl liblog libcutils\
    $(MFX_C2_SHARED_LIBS) \
        libexpat \
        libsync \
        libdrm \
        libutils \
        libhidlbase \
        libgralloctypes \
        libstagefright_foundation \
        android.hardware.media.bufferpool@2.0 \
        android.hardware.graphics.bufferqueue@2.0 \
        android.hardware.graphics.common@1.2 \
        android.hardware.graphics.mapper@4.0 \
        libsfplugin_ccodec_utils

  LOCAL_STATIC_LIBRARIES := \
    libmfx_c2_buffers

  ifneq ($(1),pure)
    MODULE_SUFFIX := _$(1)

    MSDK_IMPL := $(1)

    LOCAL_C_INCLUDES += $$(MFX_C2_INCLUDES_LIBVA)
    LOCAL_CFLAGS += $$(MFX_C2_CFLAGS_LIBVA)
    LOCAL_SHARED_LIBRARIES += libva libva-android
    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils_va
  else
    MODULE_SUFFIX := _pure

    MSDK_IMPL := sw

    LOCAL_STATIC_LIBRARIES += libmfx_c2_utils
  endif

  ifneq ($(USE_ONEVPL), true)
    LOCAL_SHARED_LIBRARIES_32 := libmfx$$(MSDK_IMPL)32
    LOCAL_SHARED_LIBRARIES_64 := libmfx$$(MSDK_IMPL)64
  endif

  LOCAL_HEADER_LIBRARIES := $$(MFX_C2_HEADER_LIBRARIES)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_c2_components$$(MODULE_SUFFIX)

  include $(BUILD_SHARED_LIBRARY)
endef

ifeq ($(MFX_C2_IMPL_HW),true)
  $(eval $(call build_components,hw))
endif

ifeq ($(MFX_C2_IMPL_SW),true)
  $(eval $(call build_components,sw))
endif

ifeq ($(MFX_C2_IMPL_PURE),true)
  $(eval $(call build_components,pure)) # pure components without libVA
endif
