/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"

/**
 * Fills mfxFrameInfo structure with default values.
 * @param[out] info structure to initialize
 */
void mfx_set_defaults_mfxFrameInfo(mfxFrameInfo* info);
/**
 * Fills mfxVideoParam with default values assuming that it will be used in
 * decoder initialization. Since parameters can be different for different
 * decoders caller should fill params->mfx.CodecId field prior calling this
 * function,
 * @param[in,out] params parameters to initialize
 */
void mfx_set_defaults_mfxVideoParam_dec(mfxVideoParam* params);
/**
 * Fills mfxVideoParam with default values assuming that it will be used in
 * vpp initialization.
 * @param[in,out] params parameters to initialize
 */
void mfx_set_defaults_mfxVideoParam_vpp(mfxVideoParam* params);
/**
 * Fills mfxVideoParam rate control method with specified value and
 * fills depended parameters to default values.
 */
mfxStatus mfx_set_RateControlMethod(mfxU16 rate_control_method, mfxVideoParam* params);
/**
 * Fills mfxVideoParam with default values assuming that it will be used in
 * encoder initialization. Since parameters can be different for different
 * encoders caller should fill params->mfx.CodecId field prior calling this
 * function,
 * @param[in,out] params parameters to initialize
 */
mfxStatus mfx_set_defaults_mfxVideoParam_enc(mfxVideoParam* params);
