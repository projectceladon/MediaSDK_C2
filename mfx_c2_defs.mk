# Purpose:
#   Defines include paths, compilation flags, etc. to build C2 plug-ins.

MFX_C_INCLUDES_C2 += $(MFX_C2_HOME)/codec2/include/

MFX_CFLAGS_C2 := \
    -std=c++14 \
    -fexceptions
