/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2017 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_c2_defs.h

*********************************************************************************/

#ifndef __MFX_C2_DEFS_H__
#define __MFX_C2_DEFS_H__

#define MFX_C2_MAX_PATH 260
#define MFX_C2_CONFIG_FILE_NAME "mfx_c2_store.conf"
#define MFX_C2_CONFIG_FILE_PATH "/etc"

#if MFX_C2_DEBUG == MFX_C2_YES

    #define MFX_C2_AUTO_TRACE(_task_name)
    #define MFX_C2_AUTO_TRACE_FUNC()

    #define MFX_C2_AUTO_TRACE_MSG(_arg)
    #define MFX_C2_AUTO_TRACE_I32(_arg)
    #define MFX_C2_AUTO_TRACE_U32(_arg)
    #define MFX_C2_AUTO_TRACE_I64(_arg)
    #define MFX_C2_AUTO_TRACE_F64(_arg)
    #define MFX_C2_AUTO_TRACE_P(_arg)
    #define MFX_C2_AUTO_TRACE_S(_arg)
    #define MFX_C2_LOG(_arg, ...)

#else // #if MFX_C2_DEBUG == MFX_C2_YES


#endif // #if MFX_C2_DEBUG == MFX_C2_YES

#endif // #ifndef __MFX_C2_DEFS_H__
