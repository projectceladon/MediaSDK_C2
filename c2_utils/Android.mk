LOCAL_PATH:= $(call my-dir)

include $(MFX_C2_HOME)/mfx_c2_env.mk

# Usage: $(call build_utils, va|pure)
define build_utils
  include $$(CLEAR_VARS)
  include $$(MFX_C2_HOME)/mfx_c2_defs.mk

  LOCAL_SRC_FILES := $$(addprefix src/, $$(notdir $$(wildcard $$(LOCAL_PATH)/src/*.cpp)))

  LOCAL_C_INCLUDES := $$(MFX_C2_INCLUDES)

  LOCAL_CFLAGS := $$(MFX_C2_CFLAGS)

  ifeq ($(1),va)
    MODULE_SUFFIX := _$(1)

    LOCAL_C_INCLUDES += $$(MFX_C2_INCLUDES_LIBVA)
    LOCAL_CFLAGS += $$(MFX_C2_CFLAGS_LIBVA)
  else
    MODULE_SUFFIX :=
  endif

  LOCAL_HEADER_LIBRARIES := $$(MFX_C2_HEADER_LIBRARIES)

  LOCAL_MODULE_TAGS := optional
  LOCAL_MODULE := libmfx_c2_utils$$(MODULE_SUFFIX)

  include $(BUILD_STATIC_LIBRARY)
endef

$(eval $(call build_utils,pure)) # utils lib without VA

$(eval $(call build_utils,va)) # utils lib with VA
