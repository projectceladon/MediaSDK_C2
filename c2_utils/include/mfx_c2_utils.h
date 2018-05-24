/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include <C2Buffer.h>
#include <C2Param.h>

c2_status_t MfxStatusToC2(mfxStatus mfx_status);

inline mfxU64 TimestampC2ToMfx(uint64_t timestamp)
{
    return timestamp * 90000 / MFX_SECOND_NS;
}

c2_status_t GetC2ConstGraphicBlock(
    const C2FrameData& buf_pack, std::unique_ptr<C2ConstGraphicBlock>* c_graph_block);

c2_status_t GetC2ConstLinearBlock(
    const C2FrameData& buf_pack, std::unique_ptr<C2ConstLinearBlock>* c_lin_block);

c2_status_t MapConstGraphicBlock(const C2ConstGraphicBlock& graph_block, c2_nsecs_t timeout,
    std::unique_ptr<const C2GraphicView>* graph_view);

c2_status_t MapGraphicBlock(C2GraphicBlock& graph_block, c2_nsecs_t timeout,
    std::unique_ptr<C2GraphicView>* graph_view);

c2_status_t MapConstLinearBlock(const C2ConstLinearBlock& block, c2_nsecs_t timeout,
    std::unique_ptr<C2ReadView>* read_view);

c2_status_t MapLinearBlock(C2LinearBlock& block, c2_nsecs_t timeout,
    std::unique_ptr<C2WriteView>* write_view);

template<typename ParamType>
C2ParamFieldValues MakeC2ParamField()
{
    ParamType p; // have to instantiate param here as C2ParamField constructor demands this
    return C2ParamFieldValues { C2ParamField(&p, &p.value), nullptr };
}

std::unique_ptr<C2SettingResult> MakeC2SettingResult(
    const C2ParamField& param_field,
    C2SettingResult::Failure failure,
    std::vector<C2ParamFieldValues>&& conflicting_fields = {},
    const C2FieldSupportedValues* supported_values = nullptr);

c2_status_t GetAggregateStatus(std::vector<std::unique_ptr<C2SettingResult>>* const failures);

bool FindC2Param(
    const std::vector<std::shared_ptr<C2ParamDescriptor>>& params_desc,
    C2Param::Index param_index);

std::unique_ptr<C2SettingResult> FindC2Param(
    const std::vector<std::shared_ptr<C2ParamDescriptor>>& params_desc,
    const C2Param* param);

bool AvcProfileAndroidToMfx(uint32_t android_value, mfxU16* mfx_value);

bool AvcProfileMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value);

bool AvcLevelAndroidToMfx(uint32_t android_value, mfxU16* mfx_value);

bool AvcLevelMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value);

bool HevcProfileAndroidToMfx(uint32_t android_value, mfxU16* mfx_value);

bool HevcProfileMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value);

bool HevcLevelAndroidToMfx(uint32_t android_value, mfxU16* mfx_value);

bool HevcLevelMfxToAndroid(mfxU16 mfx_value, uint32_t* android_value);

void InitNV12PlaneLayout(int32_t pitch, C2PlanarLayout* layout);

void InitNV12PlaneData(int32_t pitch, int32_t alloc_height, uint8_t* base, uint8_t** plane_data);

bool C2MemoryTypeToMfxIOPattern(bool input, C2MemoryType memory_type, mfxU16* io_pattern);

bool MfxIOPatternToC2MemoryType(bool input, mfxU16 io_pattern, C2MemoryType* memory_type);

int MfxFourCCToGralloc(mfxU32 fourcc);

// Gives access to prorected constructors of C2Buffer.
class C2BufferAccessor : public C2Buffer
{
    using C2Buffer::C2Buffer;
    friend C2Buffer MakeC2Buffer(const std::vector<C2ConstLinearBlock>& blocks);
    friend C2Buffer MakeC2Buffer(const std::vector<C2ConstGraphicBlock>& blocks);
};

inline C2Buffer MakeC2Buffer(const std::vector<C2ConstLinearBlock>& blocks)
{
    return C2BufferAccessor(blocks);
}

inline C2Buffer MakeC2Buffer(const std::vector<C2ConstGraphicBlock>& blocks)
{
    return C2BufferAccessor(blocks);
}
