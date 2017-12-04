# Recursively call sub-folder Android.mk

MFX_C2_HOME := $(call my-dir)

include $(call all-subdir-makefiles)
