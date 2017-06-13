/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#include "mfx_c2_utils.h"

using namespace android;

status_t MfxStatusToC2(mfxStatus mfx_status)
{
    switch(mfx_status) {
        case MFX_ERR_NONE:
            return C2_OK;

        case MFX_ERR_NULL_PTR:
        case MFX_ERR_INVALID_HANDLE:
        case MFX_ERR_INCOMPATIBLE_VIDEO_PARAM:
        case MFX_ERR_INVALID_VIDEO_PARAM:
        case MFX_ERR_INCOMPATIBLE_AUDIO_PARAM:
        case MFX_ERR_INVALID_AUDIO_PARAM:
            return C2_BAD_VALUE;

        case MFX_ERR_UNSUPPORTED:
            return C2_UNSUPPORTED;

        case MFX_ERR_NOT_FOUND:
            return C2_NOT_FOUND;

        case MFX_ERR_MORE_BITSTREAM:
        case MFX_ERR_MORE_DATA:
        case MFX_ERR_MORE_SURFACE:
        case MFX_ERR_NOT_INITIALIZED:
            return C2_BAD_STATE;

        case MFX_ERR_MEMORY_ALLOC:
        case MFX_ERR_NOT_ENOUGH_BUFFER:
        case MFX_ERR_LOCK_MEMORY:
            return C2_NO_MEMORY;

        case MFX_ERR_GPU_HANG:
            return C2_TIMED_OUT;

        case MFX_ERR_UNKNOWN:
        case MFX_ERR_UNDEFINED_BEHAVIOR:
        case MFX_ERR_DEVICE_FAILED:
        case MFX_ERR_ABORTED:
        case MFX_ERR_DEVICE_LOST:
        default:
            return C2_CORRUPTED;
    }
}
