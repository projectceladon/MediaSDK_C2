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

#define MFX_DEBUG_FILE_INIT // to define variable during this file compilation

#include "mfx_debug.h"
// MFX headers
#include <mfxdefs.h>
#include "mfx_defs.h"

#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef ANDROID
    #include <android/log.h>
#endif

#if MFX_DEBUG_FILE == MFX_DEBUG_YES
static FILE* GetDbgFile()
{
    static FILE* dbg_file = fopen(MFX_DEBUG_FILE_NAME, "w");
    return dbg_file;
}
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

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%ld.%09ld %s: %s: %s: +\n",ts.tv_sec, ts.tv_nsec, modulename, taskname, function);
        else
            fprintf(GetDbgFile(), "%ld.%09ld %d %s: %s: +\n", ts.tv_sec, ts.tv_nsec, (int)gettid(), modulename, function);
        fflush(GetDbgFile());
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

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%ld.%09ld %s: %s: %s: -\n",ts.tv_sec, ts.tv_nsec, modulename, taskname, function);
        else
            fprintf(GetDbgFile(), "%ld.%09ld %d %s: %s: -\n", ts.tv_sec, ts.tv_nsec, (int)gettid(), modulename, function);
        fflush(GetDbgFile());
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

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);

#if !defined(ANDROID) || (MFX_DEBUG_FILE == MFX_DEBUG_YES)
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%ld.%09ld %s: %s: %s: %s\n", ts.tv_sec, ts.tv_nsec, modulename, taskname, function, msg);
        else
            fprintf(GetDbgFile(), "%ld.%09ld %d %s: %s: %s\n", ts.tv_sec, ts.tv_nsec, (int)gettid(), modulename, function, msg);
        fflush(GetDbgFile());
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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: %s = %d\n", modulename, taskname, function, name, value);
        else
            fprintf(GetDbgFile(), "%s: %s: %s = %d\n", modulename, function, name, value);
        fflush(GetDbgFile());
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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: %s = 0x%x\n", modulename, taskname, function, name, value);
        else
            fprintf(GetDbgFile(), "%s: %s: %s = 0x%x\n", modulename, function, name, value);
        fflush(GetDbgFile());
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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: %s = %lld\n", modulename, taskname, function, name, value);
        else
            fprintf(GetDbgFile(), "%s: %s: %s = %lld\n", modulename, function, name, value);
        fflush(GetDbgFile());
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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: %s = %f\n", modulename, taskname, function, name, value);
        else
            fprintf(GetDbgFile(), "%s: %s: %s = %f\n", modulename, function, name, value);
        fflush(GetDbgFile());
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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: %s = %p\n", modulename, taskname, function, name, value);
        else
            fprintf(GetDbgFile(), "%s: %s: %s = %p\n", modulename, function, name, value);
        fflush(GetDbgFile());
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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: %s = '%s'\n", modulename, taskname, function, name, value);
        else
            fprintf(GetDbgFile(), "%s: %s: %s = '%s'\n", modulename, function, name, value);
        fflush(GetDbgFile());
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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: %s = '%s'\n", modulename, taskname, function, name, value);
        else
            fprintf(GetDbgFile(), "%s: %s: %s = '%s'\n", modulename, function, name, value);
        fflush(GetDbgFile());
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

MFX_HIDE std::mutex MfxTraceable::mutex_;

MFX_HIDE std::unordered_map<void*, const char*> MfxTraceable::names_;

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
    if (GetDbgFile())
    {
        if (taskname)
            fprintf(GetDbgFile(), "%s: %s: %s: time = %f ms\n", modulename, taskname, function, task_time);
        else
            fprintf(GetDbgFile(), "%s: %s: time = %f ms\n", modulename, function, task_time);
        fflush(GetDbgFile());
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
