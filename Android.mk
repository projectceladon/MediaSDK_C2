ifneq ($(filter $(BOARD_HAVE_MEDIASDK_CODEC2),true),)
  MFX_C2_HOME := $(call my-dir)

  # Recursively call sub-folder Android.mk
  include $(call all-subdir-makefiles)
endif
