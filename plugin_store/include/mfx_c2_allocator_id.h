/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2021 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2PlatformSupport.h>

enum : C2AllocatorStore::id_t {
    MFX_BUFFERQUEUE = android::C2PlatformAllocatorStore::PLATFORM_END, //0x14
};
