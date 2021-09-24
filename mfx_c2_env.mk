# Defines Media SDK targets to build. Should be included
# in all Android.mk files which define build targets prior
# to any other directives.

# Build HW C2 plugins
ifeq ($(MFX_C2_IMPL_HW),)
  MFX_C2_IMPL_HW:=true
endif

# BOARD_HAVE_MEDIASDK_SRC is not set
# BOARD_HAVE_MEDIASDK_OPEN_SOURCE is set
ifeq ($(BOARD_HAVE_MEDIASDK_SRC),true)

  # Build SW C2 plugins
  ifeq ($(MFX_C2_IMPL_SW),)
    MFX_C2_IMPL_SW:=true
  endif

  # Build PURE C2 plugins
  ifeq ($(MFX_C2_IMPL_PURE),)
    MFX_C2_IMPL_PURE:=true
  endif

endif
