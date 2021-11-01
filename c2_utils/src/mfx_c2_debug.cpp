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

#include <string.h>

#include "mfx_c2_debug.h"

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfxc2debug"

using namespace android;

#if MFX_DEBUG == MFX_DEBUG_YES

MFX_DEBUG_DEFINE_VALUE_DESC_PRINTF(
  android_c2_status_t,
  MFX_DEBUG_VALUE_DESC(C2_OK),
  MFX_DEBUG_VALUE_DESC(C2_BAD_VALUE),
  MFX_DEBUG_VALUE_DESC(C2_BAD_INDEX),
  MFX_DEBUG_VALUE_DESC(C2_CANNOT_DO),
  MFX_DEBUG_VALUE_DESC(C2_DUPLICATE),
  MFX_DEBUG_VALUE_DESC(C2_NOT_FOUND),
  MFX_DEBUG_VALUE_DESC(C2_BAD_STATE),
  MFX_DEBUG_VALUE_DESC(C2_BLOCKING),
  MFX_DEBUG_VALUE_DESC(C2_CANCELED),
  MFX_DEBUG_VALUE_DESC(C2_NO_MEMORY),
  MFX_DEBUG_VALUE_DESC(C2_REFUSED),
  MFX_DEBUG_VALUE_DESC(C2_TIMED_OUT),
  MFX_DEBUG_VALUE_DESC(C2_OMITTED),
  MFX_DEBUG_VALUE_DESC(C2_CORRUPTED),
  MFX_DEBUG_VALUE_DESC(C2_NO_INIT))

#endif // #if MFX_DEBUG == MFX_DEBUG_YES
