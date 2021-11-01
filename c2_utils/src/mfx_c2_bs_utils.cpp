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

#include <stdlib.h>
#include <assert.h>

#include "mfx_c2_bs_utils.h"
#include "mfx_c2_utils.h"

OutputBitstream::OutputBitstream(mfxU8 * buf, size_t size, bool emulationControl)
: m_pBuf(buf)
, m_ptr(buf)
, m_pBufEnd(buf + size)
, m_uBitOff(0)
, m_bEmulationControl(emulationControl)
{
    if (m_ptr < m_pBufEnd)
        *m_ptr = 0; // clear next byte
}

OutputBitstream::OutputBitstream(mfxU8 * buf, mfxU8 * bufEnd, bool emulationControl)
: m_pBuf(buf)
, m_ptr(buf)
, m_pBufEnd(bufEnd)
, m_uBitOff(0)
, m_bEmulationControl(emulationControl)
{
    if (m_ptr < m_pBufEnd)
        *m_ptr = 0; // clear next byte
}

mfxU32 OutputBitstream::GetNumBits() const
{
    return mfxU32(8 * (m_ptr - m_pBuf) + m_uBitOff);
}

void OutputBitstream::PutBit(mfxU32 bit)
{
    if (m_ptr >= m_pBufEnd)
        throw EndOfBuffer();

    mfxU8 mask = mfxU8(0xff << (8 - m_uBitOff));
    mfxU8 newBit = mfxU8(bit << (7 - m_uBitOff));
    *m_ptr = (*m_ptr & mask) | newBit;

    if (++m_uBitOff == 8)
    {
        if (m_bEmulationControl && m_ptr - 2 >= m_pBuf &&
            (*m_ptr & 0xfc) == 0 && *(m_ptr - 1) == 0 && *(m_ptr - 2) == 0)
        {
            if (m_ptr + 1 >= m_pBufEnd)
                throw EndOfBuffer();

            *(m_ptr + 1) = *(m_ptr + 0);
            *(m_ptr + 0) = 0x03;
            m_ptr++;
        }

        m_uBitOff = 0;
        m_ptr++;
        if (m_ptr < m_pBufEnd)
            *m_ptr = 0; // clear next byte
    }
}

void OutputBitstream::PutBits(mfxU32 val, mfxU32 nbits)
{
    assert(nbits <= 32);

    for (; nbits > 0; nbits--)
        PutBit((val >> (nbits - 1)) & 1);
}

void OutputBitstream::PutUe(mfxU32 val)
{
    if (val == 0)
    {
        PutBit(1);
    }
    else
    {
        val++;
        mfxU32 nbits = 1;
        while (val >> nbits)
            nbits++;

        PutBits(0, nbits - 1);
        PutBits(val, nbits);
    }
}

void OutputBitstream::PutSe(mfxI32 val)
{
    (val <= 0)
        ? PutUe(-2 * val)
        : PutUe( 2 * val - 1);
}

void OutputBitstream::PutTrailingBits()
{
    PutBit(1);
    while (m_uBitOff != 0)
        PutBit(0);
}

void OutputBitstream::PutRawBytes(mfxU8 const * begin, mfxU8 const * end)
{
    assert(m_uBitOff == 0);

    if (m_pBufEnd - m_ptr < end - begin)
        throw EndOfBuffer();

    std::copy(begin, end, m_ptr);
    m_uBitOff = 0;
    m_ptr += end - begin;

    if (m_ptr < m_pBufEnd)
        *m_ptr = 0;
}

void OutputBitstream::PutFillerBytes(mfxU8 filler, mfxU32 nbytes)
{
    assert(m_uBitOff == 0);

    if (m_ptr + nbytes > m_pBufEnd)
        throw EndOfBuffer();

    memset(m_ptr, filler, nbytes);
    m_ptr += nbytes;
}

/* temporal class definition */
class DwordPointer_
{
public:
    // Default constructor
    DwordPointer_(void)
    : m_pDest(NULL)
    , m_nByteNum(0)
    , m_iCur(0)
    {
    }

    DwordPointer_ operator = (void *pDest)
    {
        m_pDest = (mfxU32 *) pDest;
        m_nByteNum = 0;
        m_iCur = 0;

        return *this;
    }

    // Increment operator
    DwordPointer_ &operator ++ (void)
    {
        if (4 == ++m_nByteNum)
        {
            *m_pDest = m_iCur;
            m_pDest += 1;
            m_nByteNum = 0;
            m_iCur = 0;
        }
        else
            m_iCur <<= 8;

        return *this;
    }

    mfxU8 operator = (mfxU8 nByte)
    {
        m_iCur = (m_iCur & ~0x0ff) | ((mfxU32) nByte);

        return nByte;
    }

protected:
    mfxU32 *m_pDest;                                            // pointer to destination buffer
    mfxU32 m_nByteNum;                                          // number of current byte in dword
    mfxU32 m_iCur;                                              // current dword
};

class SourcePointer_
{
public:
    // Default constructor
    SourcePointer_(void)
    : m_pSource(NULL)
    , m_nZeros(0)
    , m_nRemovedBytes(0)
    {
    }

    SourcePointer_ &operator = (mfxU8 *pSource)
    {
        m_pSource = (mfxU8 *) pSource;

        m_nZeros = 0;
        m_nRemovedBytes = 0;

        return *this;
    }

    SourcePointer_ &operator ++ (void)
    {
        mfxU8 bCurByte = m_pSource[0];

        if (0 == bCurByte)
            m_nZeros += 1;
        else
        {
            if ((3 == bCurByte) && (2 <= m_nZeros))
                m_nRemovedBytes += 1;
            m_nZeros = 0;
        }

        m_pSource += 1;

        return *this;
    }

    bool IsPrevent(void)
    {
        if ((3 == m_pSource[0]) && (2 <= m_nZeros))
            return true;
        else
            return false;
    }

    operator mfxU8 (void)
    {
        return m_pSource[0];
    }

    mfxU32 GetRemovedBytes(void)
    {
        return m_nRemovedBytes;
    }

protected:
    mfxU8 *m_pSource;                                           // pointer to destination buffer
    mfxU32 m_nZeros;                                            // number of preceding zeros
    mfxU32 m_nRemovedBytes;                                     // number of removed bytes
};

void SwapMemoryAndRemovePreventingBytes(mfxU8 *pDestination, mfxU32 &nDstSize, mfxU8 *pSource, mfxU32 nSrcSize)
{
    DwordPointer_ pDst;
    SourcePointer_ pSrc;
    size_t i;

    // DwordPointer object is swapping written bytes
    // SourcePointer_ removes preventing start-code bytes

    // reset pointer(s)
    pSrc = pSource;
    pDst = pDestination;

    // first two bytes
    i = 0;
    while (i < (mfxU32) MSDK_MIN(2, nSrcSize))
    {
        pDst = (mfxU8) pSrc;
        ++pDst;
        ++pSrc;
        ++i;
    }

    // do swapping
    while (i < (mfxU32) nSrcSize)
    {
        if (false == pSrc.IsPrevent())
        {
            pDst = (mfxU8) pSrc;
            ++pDst;
        }
        ++pSrc;
        ++i;
    }

    // write padding bytes
    nDstSize = nSrcSize - pSrc.GetRemovedBytes();
    while (nDstSize & 3)
    {
        pDst = (mfxU8) (0);
        ++nDstSize;
        ++pDst;
    }
}

void BytesSwapper::SwapMemory(mfxU8 *pDestination, mfxU32 &nDstSize, mfxU8 *pSource, mfxU32 nSrcSize)
{
    SwapMemoryAndRemovePreventingBytes(pDestination, nDstSize, pSource, nSrcSize);
}