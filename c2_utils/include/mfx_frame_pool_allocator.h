/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Buffer.h>
#include <memory>

class MfxFramePoolAllocator
{
public:
    virtual void SetC2Allocator(std::shared_ptr<android::C2BlockPool> c2_allocator) = 0;
    virtual std::shared_ptr<android::C2GraphicBlock> Alloc() = 0;
    virtual void Reset() = 0;

protected: // virtual deletion prohibited
    virtual ~MfxFramePoolAllocator() = default;
};
