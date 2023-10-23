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

#include "mfx_gralloc_instance.h"
#include "mfx_c2_debug.h"
#include "mfx_c2_utils.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_gralloc_instance"


std::shared_ptr<IMfxGrallocModule> MfxGrallocInstance::m_instance = nullptr;
std::mutex MfxGrallocInstance::m_mutex;

std::shared_ptr<IMfxGrallocModule> MfxGrallocInstance::getInstance()
{
    MFX_DEBUG_TRACE_FUNC;

    if (nullptr == m_instance)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (nullptr == m_instance)
        {
#ifdef USE_GRALLOC4
            MFX_DEBUG_TRACE_MSG("using gralloc4");
            m_instance = std::make_shared<MfxGralloc4Module>();
#else
            MFX_DEBUG_TRACE_MSG("using gralloc1");
            m_instance = std::make_shared<MfxGralloc1Module>();
#endif
            if(C2_OK != m_instance->Init())
            {
                MFX_DEBUG_TRACE_MSG("MfxGrallocInstance initailization failed.");
                return nullptr;
            }
        }
    }

    return m_instance;

}
