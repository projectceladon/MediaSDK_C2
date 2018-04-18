LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/mdp_msdk-lib/android/mfx_env.mk

# Usage: $(call build_utils, va|pure)
define build_utils
  include $$(CLEAR_VARS)
  include $$(MFX_HOME)/mdp_msdk-lib/android/mfx_defs.mk
  include $(MFX_C2_HOME)/mfx_c2_defs.mk

  LOCAL_SRC_FILES := \
      $$(addprefix src/, $$(notdir $$(wildcard $$(LOCAL_PATH)/src/*.cpp)))
  LOCAL_C_INCLUDES += \
      $$(MFX_C_INCLUDES) \
      $$(MFX_C_INCLUDES_C2) \
      $(MFX_C2_HOME)/codec2/vndk

  LOCAL_CFLAGS += \
      $$(MFX_CFLAGS) \
      $$(MFX_CFLAGS_C2)

  LOCAL_MODULE_TAGS := optional

  ifeq ($(1),va)
    MODULE_SUFFIX := _$(1)

    LOCAL_C_INCLUDES += \
        $$(MFX_C_INCLUDES_LIBVA)
    LOCAL_CFLAGS += \
        $$(MFX_CFLAGS_LIBVA)
  else
    MODULE_SUFFIX :=
  endif

  LOCAL_HEADER_LIBRARIES := \
      $$(MFX_HEADER_LIBRARIES) \
      libhardware_headers

  LOCAL_MODULE := libmfx_c2_utils$$(MODULE_SUFFIX)

  include $(BUILD_STATIC_LIBRARY)
endef

$(eval $(call build_utils,pure)) # utils lib without VA

$(eval $(call build_utils,va)) # utils lib with VA
