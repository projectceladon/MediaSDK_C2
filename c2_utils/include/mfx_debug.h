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

#define MFX_DEBUG_NO  0
#define MFX_DEBUG_YES 1

#define MFX_DEBUG MFX_DEBUG_NO // enables DEBUG output
#define MFX_PERF MFX_DEBUG_NO // enables PERF output, doesn't depends on MFX_DEBUG
#define MFX_ATRACE MFX_DEBUG_NO // enables systrace

#define MFX_DEBUG_FILE MFX_DEBUG_NO // sends DEBUG and PERF output to file, otherwise to logcat

#ifndef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfxdebug"
#endif

#ifdef ANDROID
#define MFX_LOG_TAG "mediasdk_c2"
#include <utils/Log.h>
#define MFX_PRINT(LEVEL, ...) \
    __android_log_print(LEVEL, MFX_LOG_TAG, __VA_ARGS__)
#else
#define MFX_PRINT(LEVEL, ...)
#endif

#define MFX_LOG(LEVEL, _arg, ...) \
    MFX_PRINT(LEVEL, "%s: %s[line %d]: " _arg, MFX_DEBUG_MODULE_NAME, __FUNCTION__, __LINE__, ##__VA_ARGS__)

#define MFX_LOG_ERROR(_arg, ...) \
    MFX_LOG(ANDROID_LOG_ERROR, _arg, ##__VA_ARGS__)
#define MFX_LOG_INFO(_arg, ...) \
    MFX_LOG(ANDROID_LOG_INFO, _arg, ##__VA_ARGS__)

#if (MFX_DEBUG == MFX_DEBUG_YES) || (MFX_PERF == MFX_DEBUG_YES)

#ifdef ANDROID
#define MFX_DEBUG_LOG_TAG "mediasdk_c2"
#define MFX_DEBUG_LOG_LEVEL ANDROID_LOG_DEBUG
#endif

#include <stdio.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <thread>
#include "mfxdefs.h"

#if MFX_DEBUG_FILE == MFX_DEBUG_YES
  #ifdef MFX_DEBUG_FILE_INIT
    #ifndef MFX_DEBUG_FILE_NAME
      #if defined(_WIN32) || defined(_WIN64)
          #define MFX_DEBUG_FILE_NAME "C:\\mfx_c2_log.txt"
      #elif defined(ANDROID)
          #define MFX_DEBUG_FILE_NAME "/data/local/tmp/mfx_c2_log.txt"
      #else
          #define MFX_DEBUG_FILE_NAME "/tmp/mfx_c2_log.txt"
      #endif
    #endif // #ifndef MFX_DEBUG_FILE_NAME
  #endif
#endif

#endif // #if (MFX_DEBUG == MFX_DEBUG_YES) || (MFX_PERF == MFX_DEBUG_YES)

#if MFX_DEBUG == MFX_DEBUG_YES

class mfxDebugTrace
{
public:
  mfxDebugTrace(const char* _modulename, const char* _function, const char* _taskname);
  ~mfxDebugTrace(void);
  void printf_msg(const char* msg);
  void printf_i32(const char* name, mfxI32 value);
  void printf_u32(const char* name, mfxU32 value);
  void printf_i64(const char* name, mfxI64 value);
  void printf_f64(const char* name, mfxF64 value);
  void printf_p(const char* name, void* value);
  void printf_s(const char* name, const char* value);
  void printf_e(const char* name, const char* value);

protected:
  const char* modulename;
  const char* function;
  const char* taskname;

private:
  mfxDebugTrace(const mfxDebugTrace&);
  mfxDebugTrace& operator=(const mfxDebugTrace&);
};

#define MFX_DEBUG_TRACE_VAR _mfxDebugTrace

template<typename T>
struct mfxDebugValueDesc
{
  T value;
  const char* desc; // description of the value

//  inline bool operator==(const T& left, const T& right) { /* do actual comparison */ }
};

#define MFX_DEBUG_VALUE_DESC(_v) { _v, #_v }

template<typename T>
struct mfxDebugTraceValueDescCtx
{
  mfxDebugTrace& trace;
  const mfxDebugValueDesc<T>* table;
  size_t table_n;

  mfxDebugTraceValueDescCtx(
    mfxDebugTrace& _trace,
    const mfxDebugValueDesc<T>* _table,
    size_t _table_n):
      trace(_trace),
      table(_table),
      table_n(_table_n)
  {
  }
};

template<typename T>
void printf_value_from_desc(
  mfxDebugTraceValueDescCtx<T>& ctx,
  const char* name,
  T value)
{
  size_t i;
  for (i = 0; i < ctx.table_n; ++i) {
    if (value == ctx.table[i].value) {
      ctx.trace.printf_e(name, ctx.table[i].desc);
      return;
    }
  }
  // TODO alter printing function thru ctx
  ctx.trace.printf_i32(name, (mfxI32)value);
}

#define MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(TYPE,...) \
  /* declaration of the printf function*/ \
  void printf_##TYPE( \
    mfxDebugTrace& trace, \
    const char* name, \
    TYPE value);

#define MFX_DEBUG_DEFINE_VALUE_DESC_PRINTF(TYPE,...) \
  void printf_##TYPE( \
    mfxDebugTrace& trace, \
    const char* name, \
    TYPE value) \
  { \
    mfxDebugValueDesc<TYPE> t[] = \
    { \
      __VA_ARGS__ \
    }; \
    mfxDebugTraceValueDescCtx<TYPE> ctx(trace, t, sizeof(t)/sizeof(t[0])); \
    printf_value_from_desc(ctx, name, value); \
  }

#define MFX_DEBUG_TRACE(_task_name) \
  mfxDebugTrace MFX_DEBUG_TRACE_VAR(MFX_DEBUG_MODULE_NAME, __FUNCTION__, _task_name)

#define MFX_DEBUG_TRACE_FUNC \
  MFX_DEBUG_TRACE(NULL)

#define MFX_DEBUG_TRACE_MSG(_arg) \
  MFX_DEBUG_TRACE_VAR.printf_msg(_arg)

#define MFX_DEBUG_TRACE_I32(_arg) \
  MFX_DEBUG_TRACE_VAR.printf_i32(#_arg, (mfxI32)_arg)

#define MFX_DEBUG_TRACE_U32(_arg) \
  MFX_DEBUG_TRACE_VAR.printf_u32(#_arg, (mfxU32)_arg)

#define MFX_DEBUG_TRACE_I64(_arg) \
  MFX_DEBUG_TRACE_VAR.printf_i64(#_arg, (mfxI64)_arg)

#define MFX_DEBUG_TRACE_F64(_arg) \
  MFX_DEBUG_TRACE_VAR.printf_f64(#_arg, (mfxF64)_arg)

#define MFX_DEBUG_TRACE_P(_arg) \
  MFX_DEBUG_TRACE_VAR.printf_p(#_arg, (void*)_arg)

#define MFX_DEBUG_TRACE_S(_arg) \
  MFX_DEBUG_TRACE_VAR.printf_s(#_arg, _arg)

#define MFX_DEBUG_TRACE_E(_arg, _val) \
  MFX_DEBUG_TRACE_VAR.printf_e(#_arg, _val)

#define MFX_DEBUG_TRACE_PRINTF(...) \
{ \
    const unsigned int MFX_DEBUG_MAX_LOG_SIZE = 1024; \
    char MFX_DEBUG_LOG[MFX_DEBUG_MAX_LOG_SIZE]; \
    snprintf(MFX_DEBUG_LOG, MFX_DEBUG_MAX_LOG_SIZE, __VA_ARGS__); \
    MFX_DEBUG_TRACE_MSG(MFX_DEBUG_LOG); \
}

#define MFX_DEBUG_TRACE_STREAM(...) \
{ \
    std::ostringstream MFX_DEBUG_STREAM; \
    MFX_DEBUG_STREAM << __VA_ARGS__; \
    MFX_DEBUG_TRACE_MSG(MFX_DEBUG_STREAM.str().c_str()); \
}

#define mfx_enumval_eq(_e, _v) ((_e) == _v) MFX_DEBUG_TRACE_E(_e, #_v)

class MfxTraceable
{
public:
    MfxTraceable(void* instance, const char* name);

    ~MfxTraceable();

    static const char* GetName(void* instance);

private:
    static void Register(void* instance, const char* name);

    static void Unregister(void* instance);

private:
    void* instance_;
    static std::mutex mutex_;
    static std::unordered_map<void*, const char*> names_;
};

#define MFX_TRACEABLE(var) MfxTraceable traceable_##var##_ { &var, #var }

#define MFX_PTR_NAME(ptr) MfxTraceable::GetName(ptr)

#else // #if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE_VAR

#define MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(TYPE,...)
#define MFX_DEBUG_DEFINE_VALUE_DESC_PRINTF(TYPE)

#define MFX_DEBUG_TRACE(_task_name)
#define MFX_DEBUG_TRACE_FUNC

#define MFX_DEBUG_TRACE_MSG(_arg)
#define MFX_DEBUG_TRACE_I32(_arg)
#define MFX_DEBUG_TRACE_U32(_arg)
#define MFX_DEBUG_TRACE_I64(_arg)
#define MFX_DEBUG_TRACE_F64(_arg)
#define MFX_DEBUG_TRACE_P(_arg)
#define MFX_DEBUG_TRACE_S(_arg)
#define MFX_DEBUG_TRACE_E(_arg, _val)
#define MFX_DEBUG_TRACE_PRINTF(...)
#define MFX_DEBUG_TRACE_STREAM(...)

#define MFX_TRACEABLE(var)
#define MFX_PTR_NAME(ptr)

#endif // #if MFX_DEBUG == MFX_DEBUG_YES

#if MFX_PERF == MFX_DEBUG_YES

#include <chrono>

class mfxPerf
{
public:
    mfxPerf(const char* _modulename, const char* _function, const char* _taskname);
    ~mfxPerf(void);
    mfxPerf(const mfxPerf&) = delete;
    mfxPerf& operator=(const mfxPerf&) = delete;

protected:
    const char* modulename;
    const char* function;
    const char* taskname;
    std::chrono::high_resolution_clock::time_point start;
};

#define MFX_AUTO_PERF(_task_name) \
    mfxPerf _mfx_perf(MFX_DEBUG_MODULE_NAME, __FUNCTION__, _task_name)

#define MFX_AUTO_PERF_FUNC \
    MFX_AUTO_PERF(NULL)

#if !(MFX_DEBUG == MFX_DEBUG_YES)
#undef MFX_DEBUG_TRACE_FUNC
#define MFX_DEBUG_TRACE_FUNC \
  MFX_AUTO_PERF_FUNC
#endif

#else // #if MFX_PERF == MFX_DEBUG_YES

#define MFX_AUTO_PERF(_task_name)
#define MFX_AUTO_PERF_FUNC

#endif // #if MFX_PERF == MFX_DEBUG_YES

#if MFX_ATRACE == MFX_DEBUG_YES

#define ATRACE_TAG ATRACE_TAG_VIDEO
#include <utils/Trace.h>
#include <utils/String16.h>

#undef MFX_DEBUG_TRACE_FUNC
#define MFX_DEBUG_TRACE_FUNC ATRACE_CALL()

#endif
