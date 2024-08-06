// Copyright (c) 2017-2023 Intel Corporation
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

#include "mfx_c2_components_monitor.h"
#include "mfx_c2_debug.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_c2_components_monitor"

uint32_t MfxC2ComponentsMonitor::get(std::string name)
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_lock<std::mutex> lock(m_mutex);
    auto obj = m_monitor.find(name);
    if (m_monitor.end() != obj)
    {
        MFX_DEBUG_TRACE_I32(obj->second);
        return obj->second;
    }
    return 0;
}

void MfxC2ComponentsMonitor::increase(std::string name)
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_lock<std::mutex> lock(m_mutex);
    auto obj = m_monitor.find(name);
    if (m_monitor.end() == obj)
    {
        MFX_DEBUG_TRACE_STREAM("increase " << name << ", instances 0 + 1");
        m_monitor.insert(std::pair<std::string, uint32_t>(name, 1));
    }
    else
    {
        MFX_DEBUG_TRACE_STREAM("increase " << name << ", instances " << obj->second << " + 1");
        obj->second++;
    }
}

void MfxC2ComponentsMonitor::decrease(std::string name)
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_lock<std::mutex> lock(m_mutex);
    auto obj = m_monitor.find(name);
    if (m_monitor.end() == obj) // impossible
    {
        MFX_DEBUG_TRACE_MSG("can't find the record!");
        m_monitor.insert(std::pair<std::string, uint32_t>(name, 0));
    }
    else
    {
        MFX_DEBUG_TRACE_STREAM("decrease " << name << ", instances " << obj->second << " - 1");
        if (0 == obj->second)
            obj->second = 0;
        else
            obj->second--;
    }
}
