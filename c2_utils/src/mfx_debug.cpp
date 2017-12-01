/**********************************************************************************

Copyright (C) 2005-2016 Intel Corporation.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.
- Neither the name of Intel Corporation nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY INTEL CORPORATION "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL INTEL CORPORATION BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

**********************************************************************************/

#define MFX_DEBUG_FILE_INIT // to define variable during this file compilation

#include "mfx_debug.h"
// MFX headers
#include <mfxdefs.h>
#include "mfx_defs.h"

#include <string.h>

#ifdef ANDROID
    #include <android/log.h>
#endif

#if MFX_DEBUG == MFX_DEBUG_YES

static const char* g_debug_pattern[] =
{
};

/*------------------------------------------------------------------------------*/

static bool is_matched(const char* str)
{
    if (0 == MFX_GET_ARRAY_SIZE(g_debug_pattern)) return true; // match all
    if (!str) return false;

    for (int i=0; i < MFX_GET_ARRAY_SIZE(g_debug_pattern); ++i)
    {
        if (strstr(str, g_debug_pattern[i])) return true;
    }
    return false;
}

/*------------------------------------------------------------------------------*/

mfxDebugTrace::mfxDebugTrace(const char* _modulename, const char* _function, const char* _taskname)
{

    modulename = _modulename;
    function = _function;
    taskname = _taskname;

    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: +\n", modulename, taskname, function);
        else
            fprintf(g_dbg_file, "%s: %s: +\n", modulename, function);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: +", modulename, taskname, function);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: +", modulename, function);
#endif
}

/*------------------------------------------------------------------------------*/

mfxDebugTrace::~mfxDebugTrace(void)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: -\n", modulename, taskname, function);
        else
            fprintf(g_dbg_file, "%s: %s: -\n", modulename, function);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: -", modulename, taskname, function);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: -", modulename, function);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_msg(const char* msg)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s\n", modulename, taskname, function, msg);
        else
            fprintf(g_dbg_file, "%s: %s: %s\n", modulename, function, msg);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s", modulename, taskname, function, msg);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s", modulename, function, msg);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_i32(const char* name, mfxI32 value)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s = %d\n", modulename, taskname, function, name, value);
        else
            fprintf(g_dbg_file, "%s: %s: %s = %d\n", modulename, function, name, value);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s = %d", modulename, taskname, function, name, value);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s = %d", modulename, function, name, value);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_u32(const char* name, mfxU32 value)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s = 0x%x\n", modulename, taskname, function, name, value);
        else
            fprintf(g_dbg_file, "%s: %s: %s = 0x%x\n", modulename, function, name, value);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s = 0x%x", modulename, taskname, function, name, value);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s = 0x%x", modulename, function, name, value);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_i64(const char* name, mfxI64 value)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s = %lld\n", modulename, taskname, function, name, value);
        else
            fprintf(g_dbg_file, "%s: %s: %s = %lld\n", modulename, function, name, value);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s = %lld", modulename, taskname, function, name, value);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s = %lld", modulename, function, name, value);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_f64(const char* name, mfxF64 value)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s = %f\n", modulename, taskname, function, name, value);
        else
            fprintf(g_dbg_file, "%s: %s: %s = %f\n", modulename, function, name, value);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s = %f", modulename, taskname, function, name, value);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s = %f", modulename, function, name, value);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_p(const char* name, void* value)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s = %p\n", modulename, taskname, function, name, value);
        else
            fprintf(g_dbg_file, "%s: %s: %s = %p\n", modulename, function, name, value);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s = %p", modulename, taskname, function, name, value);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s = %p", modulename, function, name, value);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_s(const char* name, const char* value)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s = '%s'\n", modulename, taskname, function, name, value);
        else
            fprintf(g_dbg_file, "%s: %s: %s = '%s'\n", modulename, function, name, value);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s = %s", modulename, taskname, function, name, value);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s = %s", modulename, function, name, value);
#endif
}

/*------------------------------------------------------------------------------*/

void mfxDebugTrace::printf_e(const char* name, const char* value)
{
    if (!is_matched(function)) return;

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: %s = '%s'\n", modulename, taskname, function, name, value);
        else
            fprintf(g_dbg_file, "%s: %s: %s = '%s'\n", modulename, function, name, value);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: %s = %s", modulename, taskname, function, name, value);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s = %s", modulename, function, name, value);
#endif
}

/*------------------------------------------------------------------------------*/

MfxTraceable::MfxTraceable(void* instance, const char* name):
    instance_(instance)
{
    Register(instance, name);
}

MfxTraceable::~MfxTraceable()
{
    Unregister(instance_);
}

const char* MfxTraceable::GetName(void* instance)
{
    const char* res = nullptr;

    std::lock_guard<std::mutex> lock(mutex_);
    auto it = names_.find(instance);
    if (it != names_.end()) {
        res = it->second;
    }
    return res;
}

void MfxTraceable::Register(void* instance, const char* name)
{
    std::lock_guard<std::mutex> lock(mutex_);

    names_.emplace(instance, name);
}

void MfxTraceable::Unregister(void* instance)
{
    std::lock_guard<std::mutex> lock(mutex_);

    names_.erase(instance);
}

std::mutex MfxTraceable::mutex_;

std::unordered_map<void*, const char*> MfxTraceable::names_;

#endif // #if MFX_DEBUG == MFX_DEBUG_YES

#if MFX_PERF == MFX_DEBUG_YES

/*------------------------------------------------------------------------------*/

mfxPerf::mfxPerf(const char* _modulename, const char* _function, const char* _taskname)
{
    modulename = _modulename;
    function = _function;
    taskname = _taskname;
    start = std::chrono::high_resolution_clock::now();
}

/*------------------------------------------------------------------------------*/

mfxPerf::~mfxPerf(void)
{
    std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> task_period = end - start;
    float task_time = task_period.count();

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (g_dbg_file)
    {
        if (taskname)
            fprintf(g_dbg_file, "%s: %s: %s: time = %f ms", modulename, taskname, function, task_time);
        else
            fprintf(g_dbg_file, "%s: %s: time = %f ms", modulename, function, task_time);
        fflush(g_dbg_file);
    }
#else
    if (taskname)
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: %s: time = %f ms", modulename, taskname, function, task_time);
    else
        __android_log_print(MFX_DEBUG_LOG_LEVEL, MFX_DEBUG_LOG_TAG, "%s: %s: time = %f ms", modulename, function, task_time);
#endif
}

/*------------------------------------------------------------------------------*/

#endif // #if MFX_PERF == MFX_DEBUG_YES
