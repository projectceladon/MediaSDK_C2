// Copyright (c) 2017-2024 Intel Corporation
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

#include <stdint.h>
#include <algorithm>
#include <cutils/native_handle.h>

template <int NUM_FDS, int NUM_INTS>
class NativeHandle : protected native_handle
{
public:

    using type = NativeHandle<NUM_FDS, NUM_INTS>;

    constexpr static int numFds = NUM_FDS;

    constexpr static int numInts = NUM_INTS;

    constexpr static int arrayLength = numFds + numInts;

    static type* create() noexcept
    {
        type* handle = static_cast<type*>(native_handle_create(numFds, numInts));

        if (handle != nullptr)
        {
            std::fill(handle->data, handle->data + arrayLength, 0);
        }

        return handle;
    }
    // End of create()

    // Cast from a compatible native_handle_t.  If types are not compatible,
    // return nullptr.
    //
    static type* cast(native_handle_t* nh)
    {
        if (nh == nullptr)
        {
            return nullptr;
        }

        if (   nh->version != sizeof(native_handle_t)
            || nh->numFds != NUM_FDS || nh->numInts != NUM_INTS )
        {
            return nullptr;
        }

        return static_cast<type*>(nh);
    }
    // End of cast()

    native_handle_t* cast()
    {
        return static_cast<native_handle_t*>(this);
    }

    int close() noexcept
    {
        return native_handle_close(this);
    }

    int destroy() noexcept
    {
        return native_handle_delete(this);
    }

    static void destroy(type*& nh)
    {
        if (nh == nullptr)
        {
            return;
        }

        nh->destroy();
        nh = nullptr;
    }
    // End of destroy() (static)

    int getVersion() const noexcept
    {
        return this->version;
    }

    int* fd_begin()
    {
        return data;
    }

    const int* fd_begin() const
    {
        return data;
    }

    int* fd_end()
    {
        return data + numFds;
    }

    const int* fd_end() const
    {
        return data + numFds;
    }

    int* int_begin()
    {
        return fd_end();
    }

    const int* int_begin() const
    {
        return fd_end();
    }

    int* int_end()
    {
        return data + arrayLength;
    }

    const int* int_end() const
    {
        return data + arrayLength;
    }

protected:

    NativeHandle() = default;
    ~NativeHandle() = default;

};
// End of NativeHandle

// Integer, which "PBNH", but no 0 terminator
constexpr uint32_t PROTECTED_BUFFER_HANDLE_MAGIC = (0UL | ('H' << 24) | ('N' << 16) | ('B' << 8) | 'P');

// ProtectedBufferHandlePayload
//
// Data that is stored as integers inside of native_handle_t::data
//
#pragma pack(push, 4)
struct ProtectedBufferHandlePayload
{
    // Magic value, used for debugging
    uint32_t magic = PROTECTED_BUFFER_HANDLE_MAGIC;

    // Buffer ID (should be a random number)
    uint32_t bufferId = 0;

    // Buffer pointer on C2 side
    uint8_t* c2Buf = nullptr;

    // Buffer pointer on OEMCrypto side
    uint8_t* oecBuf = nullptr;
};
// End of ProtectedBufferHandlePayload
#pragma pack(pop)

// A native_handle with 1 file descryptor and enough space to store ProtectedBufferHandlePayload.
//
using ProtectedBufferNativeHandle = NativeHandle<1, sizeof (ProtectedBufferHandlePayload) / sizeof(int)>;

// ProtectedBufferHandle
//
class ProtectedBufferHandle : protected ProtectedBufferNativeHandle
{
public:

    static ProtectedBufferHandle* create()
    {
        ProtectedBufferHandle* handle = static_cast<ProtectedBufferHandle*>(ProtectedBufferNativeHandle::create());

        if (handle != nullptr)
        {
            handle->initMagic();
        }

        return handle;
    }
    // End of create()

    // Cast from a compatible native_handle_t.  If types are not compatible,
    // return nullptr.
    //
    static ProtectedBufferHandle* cast(native_handle_t* nh)
    {
        if (nh == nullptr)
        {
            return nullptr;
        }

        ProtectedBufferNativeHandle* pbNative = ProtectedBufferNativeHandle::cast(nh);
        if (pbNative == nullptr)
        {
            return nullptr;
        }

        return static_cast<ProtectedBufferHandle*>(pbNative);
    }
    // End of cast()

    ProtectedBufferNativeHandle* cast()
    {
        return static_cast<ProtectedBufferNativeHandle*>(this);
    }

    const ProtectedBufferNativeHandle* cast() const
    {
        return static_cast<const ProtectedBufferNativeHandle*>(this);
    }

    native_handle_t* native_handle_cast()
    {
        return static_cast<native_handle_t*>(this);
    }

    const native_handle_t* native_handle_cast() const
    {
        return static_cast<const native_handle_t*>(this);
    }

    int close() noexcept
    {
        return ProtectedBufferNativeHandle::close();
    }

    int destroy() noexcept
    {
        return ProtectedBufferNativeHandle::destroy();
    }

    int getVersion() const
    {
        return ProtectedBufferNativeHandle::getVersion();
    }

    int getFd() const
    {
        return *fd_begin();
    }

    void setFd(int fd)
    {
        *fd_begin() = fd;
    }

    bool isGoodMagic() const
    {
        return getPayload()->magic == PROTECTED_BUFFER_HANDLE_MAGIC;
    }

    uint32_t getBufferId() const
    {
        return getPayload()->bufferId;
    }

    void setBufferId(uint32_t bufferId)
    {
        getPayload()->bufferId = bufferId;
    }

    uint8_t* getC2Buf() const
    {
        return getPayload()->c2Buf;
    }

    void setC2Buf(uint8_t* c2Buf)
    {
        getPayload()->c2Buf = c2Buf;
    }

    uint8_t* getOecBuf() const
    {
        return getPayload()->oecBuf;
    }

    void setOecBuf(uint8_t* oecBuf)
    {
        getPayload()->oecBuf = oecBuf;
    }

protected:

    ProtectedBufferHandle() = default;
    ~ProtectedBufferHandle() = default;

    ProtectedBufferHandlePayload* getPayload()
    {
        return reinterpret_cast<ProtectedBufferHandlePayload*>(int_begin());
    }

    const ProtectedBufferHandlePayload* getPayload() const
    {
        return reinterpret_cast<const ProtectedBufferHandlePayload*>(int_begin());
    }

    void initMagic()
    {
        getPayload()->magic = PROTECTED_BUFFER_HANDLE_MAGIC;
    }
};
// End of ProtectedBufferHandle
