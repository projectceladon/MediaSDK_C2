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

#pragma once

#include "mfx_debug.h"
#include <C2Work.h>
#include <C2.h>

typedef android::C2Error android_C2Error;
MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(android_C2Error)

typedef android::status_t android_status_t;
MFX_DEBUG_DECLARE_VALUE_DESC_PRINTF(android_status_t)

#if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE_android_C2Error(_e) printf_android_C2Error(MFX_DEBUG_TRACE_VAR, #_e, _e)
#define MFX_DEBUG_TRACE_android_status_t(_e) printf_android_status_t(MFX_DEBUG_TRACE_VAR, #_e, _e)

#else // #if MFX_DEBUG == MFX_DEBUG_YES

#define MFX_DEBUG_TRACE_android_C2Error(_e)
#define MFX_DEBUG_TRACE_android_status_t(_e)

#endif // #if MFX_DEBUG == MFX_DEBUG_YES
