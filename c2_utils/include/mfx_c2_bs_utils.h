// Copyright (c) 2013-2021 Intel Corporation
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

#ifndef __MFX_C2_BS_UTILS_H__
#define __MFX_C2_BS_UTILS_H__

#include <exception>
#include "mfxstructures.h"

#define MSDK_MAX(A, B)                           (((A) > (B)) ? (A) : (B))
#define MSDK_MIN(A, B)                           (((A) < (B)) ? (A) : (B))

#define SAMPLE_ASSERT(x)

class EndOfBuffer : public std::exception
{
public:
    EndOfBuffer() : std::exception() {}
};

class OutputBitstream
{
public:
    OutputBitstream(mfxU8 * buf, size_t size, bool emulationControl = true);
    OutputBitstream(mfxU8 * buf, mfxU8 * bufEnd, bool emulationControl = true);

    mfxU32 GetNumBits() const;

    void PutBit(mfxU32 bit);
    void PutBits(mfxU32 val, mfxU32 nbits);
    void PutUe(mfxU32 val);
    void PutSe(mfxI32 val);
    void PutRawBytes(mfxU8 const * begin, mfxU8 const * end); // startcode emulation is not controlled
    void PutFillerBytes(mfxU8 filler, mfxU32 nbytes);         // startcode emulation is not controlled
    void PutTrailingBits();

private:
    mfxU8 * m_pBuf;
    mfxU8 * m_ptr;
    mfxU8 * m_pBufEnd;
    mfxU32  m_uBitOff;
    bool    m_bEmulationControl;
};

class BytesSwapper
{
public:
    static void SwapMemory(mfxU8 *pDestination, mfxU32 &nDstSize, mfxU8 *pSource, mfxU32 nSrcSize);
};

#endif // __MFX_C2_BS_UTILS_H__
