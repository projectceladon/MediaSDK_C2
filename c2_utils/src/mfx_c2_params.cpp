/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2018 Intel Corporation. All Rights Reserved.

*********************************************************************************/

// The sole purpose of this cpp file is to define global reflection parameters
// for vendor types.

#include <C2Config.h> // Include before define __C2_GENERATE_GLOBAL_VARS__
// to avoid generation of global reflection variables _FIELD_LIST
// for C2 standard structures. Those variables should be used from vndk.

#define __C2_GENERATE_GLOBAL_VARS__ // Define before mfx_c2_params.h
// to generate global reflection variables for vendor types.

#include "mfx_c2_params.h"
