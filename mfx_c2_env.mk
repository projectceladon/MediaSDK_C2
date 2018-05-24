# Defines Media SDK targets to build. Should be included
# in all Android.mk files which define build targets prior
# to any other directives.

# Build HW C2 plugins
ifeq ($(MFX_C2_IMPL_HW),)
  MFX_C2_IMPL_HW:=true
endif

# Build SW C2 plugins
ifneq ($(BOARD_HAVE_MEDIASDK_OPEN_SOURCE),true)
  ifeq ($(MFX_C2_IMPL_SW),)
    MFX_C2_IMPL_SW:=true
  endif
endif

# Build PURE C2 plugins
ifneq ($(BOARD_HAVE_MEDIASDK_OPEN_SOURCE),true)
  ifeq ($(MFX_C2_IMPL_PURE),)
    MFX_C2_IMPL_PURE:=true
  endif
endif
