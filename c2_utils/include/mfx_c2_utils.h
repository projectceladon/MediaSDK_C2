/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include <C2Buffer.h>

android::status_t MfxStatusToC2(mfxStatus mfx_status);

android::status_t GetC2ConstGraphicBlock(
    const android::C2BufferPack& buf_pack, std::unique_ptr<android::C2ConstGraphicBlock>* c_graph_block);

android::status_t GetC2ConstLinearBlock(
    const android::C2BufferPack& buf_pack, std::unique_ptr<android::C2ConstLinearBlock>* c_lin_block);

android::status_t MapConstGraphicBlock(
    const android::C2ConstGraphicBlock& c_graph_block, nsecs_t timeout, const uint8_t** data);

android::status_t MapGraphicBlock(
    android::C2GraphicBlock& graph_block, nsecs_t timeout, uint8_t** data);

android::status_t MapConstLinearBlock(
    const android::C2ConstLinearBlock& block, nsecs_t timeout, const uint8_t** data);

android::status_t MapLinearBlock(
    android::C2LinearBlock& block, nsecs_t timeout, uint8_t** data);
