# Purpose:
#   Defines include paths, compilation flags, etc. to build Media SDK targets.
#
# Defined variables:
#   MFX_C2_CFLAGS - common flags for all targets
#   MFX_C2_CFLAGS_LIBVA - LibVA support flags (to build apps with or without LibVA support)
#   MFX_C2_INCLUDES - common include paths for all targets
#   MFX_C2_INCLUDES_LIBVA - include paths to LibVA headers
#   MFX_C2_HEADER_LIBRARIES - common imported headers for all targets
#   MFX_C2_LDFLAGS - common link flags for all targets

include $(MFX_C2_HOME)/mfx_c2_env.mk

# =============================================================================
# Common definitions

MFX_C2_CFLAGS := -DANDROID

# Use oneVPL API
USE_ONEVPL := true
ifeq ($(USE_ONEVPL), true)
MFX_C2_CFLAGS += -DMFX_VERSION=2003
endif

# Android version preference:
ifneq ($(filter 11 11.% R ,$(PLATFORM_VERSION)),)
  MFX_ANDROID_VERSION:= MFX_R
endif
ifneq ($(filter 10 10.% Q ,$(PLATFORM_VERSION)),)
  MFX_ANDROID_VERSION:= MFX_Q
endif
ifneq ($(filter 9 9.% P ,$(PLATFORM_VERSION)),)
  MFX_ANDROID_VERSION:= MFX_P
endif
ifneq ($(filter 8.% O ,$(PLATFORM_VERSION)),)
  ifneq ($(filter 8.0.%,$(PLATFORM_VERSION)),)
    MFX_ANDROID_VERSION:= MFX_O
  else
    MFX_ANDROID_VERSION:= MFX_O_MR1
  endif
endif
ifneq ($(filter 7.% N ,$(PLATFORM_VERSION)),)
  MFX_ANDROID_VERSION:= MFX_N
endif
ifneq ($(filter 6.% M ,$(PLATFORM_VERSION)),)
  MFX_ANDROID_VERSION:= MFX_MM
endif
ifneq ($(filter 5.% L ,$(PLATFORM_VERSION)),)
  MFX_ANDROID_VERSION:= MFX_LD
endif

ifeq ($(MFX_ANDROID_PLATFORM),)
  MFX_ANDROID_PLATFORM:=$(TARGET_BOARD_PLATFORM)
endif

# Passing Android-dependency information to the code
MFX_C2_CFLAGS += \
  -DMFX_ANDROID_VERSION=$(MFX_ANDROID_VERSION) \
  -DMFX_ANDROID_PLATFORM=$(MFX_ANDROID_PLATFORM)

ifeq ($(BOARD_USES_GRALLOC1),true)
  # plugins should use PRIME buffer descriptor since Android P
  ifneq ($(filter MFX_R ,$(MFX_ANDROID_VERSION)),)
      MFX_C2_CFLAGS += -DMFX_C2_USE_PRIME
  endif
  ifneq ($(filter MFX_Q ,$(MFX_ANDROID_VERSION)),)
      MFX_C2_CFLAGS += -DMFX_C2_USE_PRIME
  endif
  ifneq ($(filter MFX_P ,$(MFX_ANDROID_VERSION)),)
      MFX_C2_CFLAGS += -DMFX_C2_USE_PRIME
  endif
else
  $(error "Required GRALLOC1 support")
endif

# MFX_BUFFER_QUEUE := true
ifeq ($(MFX_BUFFER_QUEUE),true)
    MFX_C2_CFLAGS += -DMFX_BUFFER_QUEUE
endif

#  Security
MFX_C2_CFLAGS += \
  -fstack-protector-strong \
  -fPIE -fPIC \
  -O2 -D_FORTIFY_SOURCE=2 \
  -Wno-error \
  -Wno-deprecated-declarations \
  -fexceptions -std=c++17

# LibVA support.
MFX_C2_CFLAGS_LIBVA := -DLIBVA_SUPPORT -DLIBVA_ANDROID_SUPPORT

ifneq ($(filter $(MFX_ANDROID_VERSION), MFX_O),)
  MFX_C2_CFLAGS_LIBVA += -DANDROID_O
endif

# Setting usual paths to include files
MFX_C2_INCLUDES := \
  $(LOCAL_PATH)/include

MFX_C2_SHARED_LIBS := libcodec2_vndk

ifeq ($(BOARD_USES_GRALLOC1),true)
  MFX_C2_INCLUDES += $(INTEL_MINIGBM)/cros_gralloc
endif

MFX_C2_INCLUDES_LIBVA := $(TARGET_OUT_HEADERS)/libva

ifeq ($(USE_ONEVPL), true)
  # Setting oneVPL imported headers
  MFX_C2_HEADER_LIBRARIES := libvpl_headers libcodec2_headers
  MFX_C2_SHARED_LIBS += libvpl
else
  # Setting MediaSDK imported headers
  MFX_C2_HEADER_LIBRARIES := libmfx_headers libcodec2_headers
endif

# Setting usual imported headers
MFX_C2_HEADER_LIBRARIES += \
  libutils_headers \
  libhardware_headers   # It's here due to <hardware/gralloc.h> include. Need to remove when the header will be removed

# Setting usual link flags
MFX_C2_LDFLAGS := \
  -z noexecstack \
  -z relro -z now

MFX_C2_EXE_LDFLAGS := $(MFX_C2_LDFLAGS) -pie

# Setting vendor
LOCAL_MODULE_OWNER := intel

# Moving executables to proprietary location
LOCAL_PROPRIETARY_MODULE := true
