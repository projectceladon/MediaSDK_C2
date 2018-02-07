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
#include <C2Param.h>

android::c2_status_t MfxStatusToC2(mfxStatus mfx_status);

inline mfxU64 TimestampC2ToMfx(uint64_t timestamp)
{
    return timestamp * 90000 / MFX_SECOND_NS;
}

android::c2_status_t GetC2ConstGraphicBlock(
    const android::C2BufferPack& buf_pack, std::unique_ptr<android::C2ConstGraphicBlock>* c_graph_block);

android::c2_status_t GetC2ConstLinearBlock(
    const android::C2BufferPack& buf_pack, std::unique_ptr<android::C2ConstLinearBlock>* c_lin_block);

android::c2_status_t MapConstGraphicBlock(const android::C2ConstGraphicBlock& graph_block, nsecs_t timeout,
    std::unique_ptr<const android::C2GraphicView>* graph_view);

android::c2_status_t MapGraphicBlock(android::C2GraphicBlock& graph_block, nsecs_t timeout,
    std::unique_ptr<android::C2GraphicView>* graph_view);

android::c2_status_t MapConstLinearBlock(
    const android::C2ConstLinearBlock& block, nsecs_t timeout, const uint8_t** data);

android::c2_status_t MapLinearBlock(
    android::C2LinearBlock& block, nsecs_t timeout, uint8_t** data);

template<typename ParamType>
android::C2ParamField MakeC2ParamField()
{
    ParamType p; // have to instantiate param here as C2ParamField constructor demands this
    return android::C2ParamField(&p, &p.value);
}

std::unique_ptr<android::C2SettingResult> MakeC2SettingResult(
    const android::C2ParamField& param_field,
    android::C2SettingResult::Failure failure,
    std::initializer_list<android::C2ParamField> conflicting_fields = {},
    const android::C2FieldSupportedValues* supported_values = nullptr);

android::c2_status_t GetAggregateStatus(std::vector<std::unique_ptr<android::C2SettingResult>>* const failures);

bool FindC2Param(
    const std::vector<std::shared_ptr<android::C2ParamDescriptor>>& params_desc,
    android::C2Param::Type param_type);

std::unique_ptr<android::C2SettingResult> FindC2Param(
    const std::vector<std::shared_ptr<android::C2ParamDescriptor>>& params_desc,
    const android::C2Param* param);

bool AvcProfileAndroidToMfx(uint32_t android_value, mfxU16* mfx_value);

bool AvcProfileMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value);

bool AvcLevelAndroidToMfx(uint32_t android_value, mfxU16* mfx_value);

bool AvcLevelMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value);

void InitNV12PlaneLayout(int32_t pitch, int32_t alloc_height, android::C2PlanarLayout* layout);

bool C2MemoryTypeToMfxIOPattern(bool input, android::C2MemoryType memory_type, mfxU16* io_pattern);

bool MfxIOPatternToC2MemoryType(bool input, mfxU16 io_pattern, android::C2MemoryType* memory_type);

int MfxFourCCToGralloc(mfxU32 fourcc);
