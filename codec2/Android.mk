LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES := C2.cpp

LOCAL_C_INCLUDES := \
    $(TOP)/frameworks/av/media/libstagefright/codec2/include \
    $(TOP)/frameworks/native/include/media/hardware

LOCAL_CFLAGS := -Werror -Wall
LOCAL_CLANG := true
LOCAL_SANITIZE := unsigned-integer-overflow signed-integer-overflow

LOCAL_MODULE:= libstagefright_codec2_mfx # as module conflicts with the one from android O tree

#include $(BUILD_SHARED_LIBRARY) currently no need for this module
