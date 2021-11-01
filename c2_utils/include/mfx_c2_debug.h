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

#include "mfx_debug.h"
#include <C2Work.h>
#include <C2.h>

typedef c2_status_t android_c2_status_t;
MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(android_c2_status_t)

#if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__android_c2_status_t(_e) printf_android_c2_status_t(MFX_DEBUG_TRACE_VAR, #_e, _e)

#else // #if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE__android_c2_status_t(_e)

#endif // #if MFX_DEBUG == MFX_DEBUG_YES
