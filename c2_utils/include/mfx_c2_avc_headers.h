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

#ifndef __AVC_HEADERS_H
#define __AVC_HEADERS_H

#include <vector>
#include "mfx_c2_avc_structures.h"

namespace AVCParser
{

template <typename T>
class HeaderSet
{
public:

    HeaderSet()
        : m_nCurrentID(-1)
    {
    }

    ~HeaderSet()
    {
        Reset();
    }

    void AddHeader(T* hdr)
    {
        mfxU32 id = hdr->GetID();
        if (id >= m_headers.size())
        {
            m_headers.resize(id + 1);
        }

        if (m_headers[id])
        {
            delete m_headers[id];
            m_headers[id]=0;
        }

        m_headers[id] = new T();
        *(m_headers[id]) = *hdr;
    }

    T * GetHeader(mfxU32 id)
    {
        if (id >= m_headers.size())
            return 0;

        return m_headers[id];
    }

    void RemoveHeader(mfxU32 id)
    {
        if (id >= m_headers.size())
            return;

        delete m_headers[id];
        m_headers[id] = 0;
    }

    void RemoveHeader(T * hdr)
    {
        if (!hdr)
            return;

        RemoveHeader(hdr->GetID());
    }

    const T * GetHeader(mfxU32 id) const
    {
        if (id >= m_headers.size())
            return 0;

        return m_headers[id];
    }

    void Reset()
    {
        for (mfxU32 i = 0; i < m_headers.size(); i++)
        {
            delete m_headers[i];
            m_headers[i]=0;
        }
    }

    void SetCurrentID(mfxU32 id)
    {
        m_nCurrentID = id;
    }

    mfxI32 GetCurrrentID() const
    {
        return m_nCurrentID;
    }

    T * GetCurrentHeader()
    {
        if (m_nCurrentID == -1)
            return 0;

        return GetHeader(m_nCurrentID);
    }

    const T * GetCurrentHeader() const
    {
        if (m_nCurrentID == -1)
            return 0;

        return GetHeader(m_nCurrentID);
    }

private:
    std::vector<T*>           m_headers;
    mfxI32                    m_nCurrentID;
};

/****************************************************************************************************/
// Headers stuff
/****************************************************************************************************/
class AVCHeaders
{
public:

    void Reset()
    {
        m_SeqParams.Reset();
        m_SeqExParams.Reset();
        m_SeqParamsMvcExt.Reset();
        m_PicParams.Reset();
        m_SEIParams.Reset();
    }

    HeaderSet<AVCSeqParamSet>             m_SeqParams;
    HeaderSet<AVCSeqParamSetExtension>    m_SeqExParams;
    HeaderSet<AVCSeqParamSet>             m_SeqParamsMvcExt;
    HeaderSet<AVCPicParamSet>             m_PicParams;
    HeaderSet<AVCSEIPayLoad>              m_SEIParams;
    AVCNalExtension                       m_nalExtension;
};

} //namespace AVCParser

#endif // __AVC_HEADERS_H
