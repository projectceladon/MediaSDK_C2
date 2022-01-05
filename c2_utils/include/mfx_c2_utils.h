// Copyright (c) 2017-2021 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

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

std::shared_ptr<C2Buffer> CreateGraphicBuffer(
    const std::shared_ptr<C2GraphicBlock> &block, const C2Rect &crop);

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

bool Vp9ProfileAndroidToMfx(C2Config::profile_t android_value, mfxU16* mfx_value);

bool Vp9ProfileMfxToAndroid(mfxU16 mfx_value, C2Config::profile_t* android_value);

void InitNV12PlaneLayout(uint32_t pitches[C2PlanarLayout::MAX_NUM_PLANES], C2PlanarLayout* layout);

void InitNV12PlaneData(int32_t pitch_y, int32_t alloc_height, uint8_t* base, uint8_t** plane_data);

bool C2MemoryTypeToMfxIOPattern(bool input, C2MemoryType memory_type, mfxU16* io_pattern);

bool MfxIOPatternToC2MemoryType(bool input, mfxU16 io_pattern, C2MemoryType* memory_type);

int MfxFourCCToGralloc(mfxU32 fourcc, bool using_video_memory = true);

bool IsYUV420(const C2GraphicView &view);

bool IsNV12(const C2GraphicView &view);

bool IsI420(const C2GraphicView &view);

bool IsYV12(const C2GraphicView &view);

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

// Writes YUV format frame to file.It useful for debug.
class YUVWriter
{
public:
    YUVWriter(const std::string& dir,
        const std::vector<std::string>& sub_dirs, const std::string& name);

    //TODO:
    //Only support NV12 for now.
    void Write(const uint8_t* yuv_data, int stride, int height, int frameIndex/*, int fourCC*/);

private:
    std::ofstream stream_;
};

//declare used extension buffers
template<class T>
struct mfx_ext_buffer_id{};
template<>struct mfx_ext_buffer_id<mfxExtCodingOptionSPSPPS> {
    enum {id = MFX_EXTBUFF_CODING_OPTION_SPSPPS};
};
template<>struct mfx_ext_buffer_id<mfxExtCodingOptionVPS> {
    enum {id = MFX_EXTBUFF_CODING_OPTION_VPS};
};
template<>struct mfx_ext_buffer_id<mfxExtVP9Param> {
    enum {id = MFX_EXTBUFF_VP9_PARAM};
};

template <typename R>
struct ExtParamAccessor
{
private:
    using mfxExtBufferDoublePtr = mfxExtBuffer**;
public:
    mfxU16& NumExtParam;
    mfxExtBufferDoublePtr& ExtParam;
    ExtParamAccessor(const R& r):
        NumExtParam(const_cast<mfxU16&>(r.NumExtParam)),
        ExtParam(const_cast<mfxExtBufferDoublePtr&>(r.ExtParam)) {}
};

/** ExtBufHolder is an utility class which
 *  provide interface for mfxExtBuffer objects management in any mfx structure (e.g. mfxVideoParam)
 */
template<typename T>
class ExtBufHolder : public T
{
public:
    ExtBufHolder() : T()
    {
        m_ext_buf.reserve(g_max_num_ext_buffers);
    }

    ~ExtBufHolder() // only buffers allocated by wrapper can be released
    {
        for (auto it = m_ext_buf.begin(); it != m_ext_buf.end(); it++ )
        {
            delete [] (mfxU8*)(*it);
        }
    }

    ExtBufHolder(const ExtBufHolder& ref)
    {
        m_ext_buf.reserve(g_max_num_ext_buffers);
        *this = ref; // call to operator=
    }

    ExtBufHolder& operator=(const ExtBufHolder& ref)
    {
        const T* src_base = &ref;
        return operator=(*src_base);
    }

    ExtBufHolder(const T& ref)
    {
        *this = ref; // call to operator=
    }

    ExtBufHolder& operator=(const T& ref)
    {
        // copy content of main structure type T
        T* dst_base = this;
        const T* src_base = &ref;
        *dst_base = *src_base;

        //remove all existing extension buffers
        ClearBuffers();

        const auto ref_ = ExtParamAccessor<T>(ref);

        //reproduce list of extension buffers and copy its content
        for (size_t i = 0; i < ref_.NumExtParam; ++i)
        {
            const auto src_buf = ref_.ExtParam[i];
            if (!src_buf) throw std::range_error("Null pointer attached to source ExtParam");
            if (!IsCopyAllowed(src_buf->BufferId))
            {
                auto msg = "Deep copy of '" + Fourcc2Str(src_buf->BufferId) + "' extBuffer is not allowed";
                throw std::system_error(msg);
            }

            // 'false' below is because here we just copy extBuffer's one by one
            auto dst_buf = AddExtBuffer(src_buf->BufferId, src_buf->BufferSz, false);
            // copy buffer content w/o restoring its type
            memcpy((void*)dst_buf, (void*)src_buf, src_buf->BufferSz);
        }

        return *this;
    }

    ExtBufHolder(ExtBufHolder &&)             = default;
    ExtBufHolder & operator= (ExtBufHolder&&) = default;

    // Always returns a valid pointer or throws an exception
    template<typename TB>
    TB* AddExtBuffer()
    {
        mfxExtBuffer* b = AddExtBuffer(mfx_ext_buffer_id<TB>::id, sizeof(TB));
        return (TB*)b;
    }

    template<typename TB>
    void RemoveExtBuffer()
    {
        auto it = std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(mfx_ext_buffer_id<TB>::id));
        if (it != m_ext_buf.end())
        {
            delete [] (mfxU8*)(*it);
            it = m_ext_buf.erase(it);

            RefreshBuffers();
        }
    }

    template <typename TB>
    TB* GetExtBuffer(uint32_t fieldId = 0) const
    {
        return (TB*)FindExtBuffer(mfx_ext_buffer_id<TB>::id, fieldId);
    }

    template <typename TB>
    operator TB*()
    {
        return (TB*)FindExtBuffer(mfx_ext_buffer_id<TB>::id, 0);
    }

    template <typename TB>
    operator TB*() const
    {
        return (TB*)FindExtBuffer(mfx_ext_buffer_id<TB>::id, 0);
    }

private:

    mfxExtBuffer* AddExtBuffer(mfxU32 id, mfxU32 size)
    {
        if (!size || !id)
            throw std::range_error("AddExtBuffer: wrong size or id!");

        auto it = std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(id));
        if (it == m_ext_buf.end())
        {
            auto buf = (mfxExtBuffer*)new mfxU8[size];
            memset(buf, 0, size);
            m_ext_buf.push_back(buf);

            buf->BufferId = id;
            buf->BufferSz = size;

            RefreshBuffers();
            return m_ext_buf.back();
        }

        return *it;
    }

    mfxExtBuffer* FindExtBuffer(mfxU32 id, uint32_t fieldId) const
    {
        auto it = std::find_if(m_ext_buf.begin(), m_ext_buf.end(), CmpExtBufById(id));
        if (fieldId && it != m_ext_buf.end())
        {
            ++it;
            return it != m_ext_buf.end() ? *it : nullptr;
        }
        return it != m_ext_buf.end() ? *it : nullptr;
    }

    void RefreshBuffers()
    {
        auto this_ = ExtParamAccessor<T>(*this);
        this_.NumExtParam = static_cast<mfxU16>(m_ext_buf.size());
        this_.ExtParam    = this_.NumExtParam ? m_ext_buf.data() : nullptr;
    }

    void ClearBuffers()
    {
        if (m_ext_buf.size())
        {
            for (auto it = m_ext_buf.begin(); it != m_ext_buf.end(); it++ )
            {
                delete [] (mfxU8*)(*it);
            }
            m_ext_buf.clear();
        }
        RefreshBuffers();
    }

    bool IsCopyAllowed(mfxU32 id)
    {
        // TODO: Epand the list when necessery
        static const mfxU32 allowed[] = {
            MFX_EXTBUFF_CODING_OPTION,
            MFX_EXTBUFF_CODING_OPTION2,
            MFX_EXTBUFF_CODING_OPTION3,
            MFX_EXTBUFF_HEVC_PARAM,
            MFX_EXTBUFF_VP9_PARAM,
        };

        auto it = std::find_if(std::begin(allowed), std::end(allowed),
                               [&id](const mfxU32 allowed_id)
                               {
                                   return allowed_id == id;
                               });
        return it != std::end(allowed);
    }

    struct CmpExtBufById
    {
        mfxU32 id;

        CmpExtBufById(mfxU32 _id)
            : id(_id)
        { };

        bool operator () (mfxExtBuffer* b)
        {
            return  (b && b->BufferId == id);
        };
    };

    static std::string Fourcc2Str(mfxU32 fourcc)
    {
        std::string s;
        for (size_t i = 0; i < 4; i++)
        {
            s.push_back(*(i + (char*)&fourcc));
        }
        return s;
    }

    std::vector<mfxExtBuffer*> m_ext_buf;
};

using MfxVideoParamsWrapper = ExtBufHolder<mfxVideoParam>;
