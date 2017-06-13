LOCAL_PATH:= $(call my-dir)

include $(MFX_HOME)/android/mfx_env.mk

# Usage: $(call build_utils, va|pure)
define build_utils
  include $$(CLEAR_VARS)
  include $$(MFX_HOME)/android/mfx_defs.mk
  LOCAL_SRC_FILES := \
      $$(addprefix src/, $$(notdir $$(wildcard $$(LOCAL_PATH)/src/*.cpp)))
  MFX_C_INCLUDES_C2 := $$(LOCAL_PATH)/../codec2/include/
  LOCAL_C_INCLUDES += \
      $$(MFX_C_INCLUDES) \
      $$(MFX_C_INCLUDES_C2)
  LOCAL_CFLAGS += \
      $$(MFX_CFLAGS) \
      -std=c++14 \
      -fexceptions
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

  LOCAL_MODULE := libmfx_c2_utils$$(MODULE_SUFFIX)

  include $(BUILD_STATIC_LIBRARY)
endef

$(eval $(call build_utils,pure)) # utils lib without VA

$(eval $(call build_utils,va)) # utils lib with VA
