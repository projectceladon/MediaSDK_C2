/**********************************************************************************

Copyright(c) 2005-2019 Intel Corporation. All Rights Reserved.

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
