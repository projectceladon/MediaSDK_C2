/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2011-2017 Intel Corporation. All Rights Reserved.

*********************************************************************************

File: mfx_c2.h
  To contain functions to be accessed from the module.

Defined functions:
  getComponentStore
Defined help functions:

*********************************************************************************/
#pragma once

#include <C2Component.h>

// TODO: Codec 2.0 doc is still unclear about acquiring component store - use this for now
android::c2_status_t GetC2ComponentStore(std::shared_ptr<android::C2ComponentStore>* const componentStore);
