LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
        C2.cpp    \

LOCAL_C_INCLUDES += \
        $(TOP)/frameworks/av/media/libstagefright/codec2/include \
        $(TOP)/frameworks/native/include/media/hardware \

LOCAL_MODULE:= libstagefright_codec2_mfx # as module conflicts with the one from android O tree
LOCAL_CFLAGS += -Werror -Wall
LOCAL_CLANG := true
LOCAL_SANITIZE := unsigned-integer-overflow signed-integer-overflow

#include $(BUILD_SHARED_LIBRARY) currently no need for this module

################################################################################

include $(call all-makefiles-under,$(LOCAL_PATH))
