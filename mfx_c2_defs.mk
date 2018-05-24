# Purpose:
#   Defines include paths, compilation flags, etc. to build C2 plug-ins.

ifeq ($(USE_MOCK_CODEC2),)
    USE_MOCK_CODEC2 = false
endif

MFX_INCLUDES_C2 += $(MFX_C2_HOME)/codec2/include/

ifeq ($(USE_MOCK_CODEC2),true)
    MFX_INCLUDES_C2 += $(MFX_C2_HOME)/mock/codec2/include
    MFX_STATIC_LIBS_C2 := libmfx_mock_codec2
else
    MFX_INCLUDES_C2 += $(MFX_C2_HOME)/codec2/vndk/include
    MFX_SHARED_LIBS_C2 := libstagefright_codec2_vndk_mfx
endif

MFX_CFLAGS_C2 := \
    -std=c++14 \
    -fexceptions
