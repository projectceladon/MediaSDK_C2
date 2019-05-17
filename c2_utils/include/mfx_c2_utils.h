/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include <C2Buffer.h>
#include <C2Param.h>
#include <fstream>

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

template<typename T, typename ...Args>
std::unique_ptr<T> AllocUniqueString(const Args(&... args), const char *value)
{
    size_t len = strlen(value) + 1;
    std::unique_ptr<T> res = T::AllocUnique(len, args...);
    strcpy(res->m.value, value);
    return res;
}

bool AvcProfileAndroidToMfx(C2Config::profile_t android_value, mfxU16* mfx_value);

bool AvcProfileMfxToAndroid(mfxU16 mfx_value, C2Config::profile_t* android_value);

bool AvcLevelAndroidToMfx(C2Config::level_t android_value, mfxU16* mfx_value);

bool AvcLevelMfxToAndroid(mfxU16 mfx_value, C2Config::level_t* android_value);

bool HevcProfileAndroidToMfx(C2Config::profile_t android_value, mfxU16* mfx_value);

bool HevcProfileMfxToAndroid(mfxU16 mfx_value, C2Config::profile_t* android_value);

bool HevcLevelAndroidToMfx(C2Config::level_t android_value, mfxU16* mfx_value);

bool HevcLevelMfxToAndroid(mfxU16 mfx_value, C2Config::level_t* android_value);

void InitNV12PlaneLayout(uint32_t pitches[C2PlanarLayout::MAX_NUM_PLANES], C2PlanarLayout* layout);

void InitNV12PlaneData(int32_t pitch_y, int32_t alloc_height, uint8_t* base, uint8_t** plane_data);

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

bool operator==(const C2PlaneInfo& p0, const C2PlaneInfo& p1);

bool operator==(const C2PlanarLayout& src, const C2PlanarLayout& dst);

c2_status_t CopyGraphicView(const C2GraphicView* src, C2GraphicView* dst);

std::string FormatHex(const uint8_t* data, size_t len);

// Writes binary buffers to file.
class BinaryWriter
{
public:
    // File named <name> is created/overwritten in: dir/<sub_dirs[0]>/.../<sub_dirs[N-1]>
    // Folders are created if missing.
    // So if a file with path Dir/SubDir1/SubDir2/File.txt is supposed to be written,
    // then it should be passed as: BinaryWriter( "Dir", { "SubDir1", "SubDir2" }, "File.txt")
    BinaryWriter(const std::string& dir,
        const std::vector<std::string>& sub_dirs, const std::string& name);

public:
    void Write(const uint8_t* data, size_t length)
    {
        stream_.write((const char*)data, length);
    }

private:
    std::ofstream stream_;
};
