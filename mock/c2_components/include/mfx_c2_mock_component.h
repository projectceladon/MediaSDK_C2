/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include "mfx_c2_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_cmd_queue.h"

class MfxC2MockComponent : public MfxC2Component
{
public:
    enum Type {
        Encoder,
        Decoder
    };
protected:
    MfxC2MockComponent(const android::C2String name, int flags, Type type);

    MFX_CLASS_NO_COPY(MfxC2MockComponent)

public:
    static void RegisterClass(MfxC2ComponentsRegistry& registry);

protected: // android::C2Component
    status_t queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items) override;

protected:
    android::status_t Init() override;

    android::status_t DoStart() override;

    android::status_t DoStop() override;

private:
    // Allocates linear block of the length as input and copies input there.
    status_t CopyGraphicToLinear(const android::C2BufferPack& input,
        const std::shared_ptr<android::C2BlockAllocator>& allocator,
        std::shared_ptr<android::C2Buffer>* out_buffer);
    // Allocates graphic block of the length as input and copies input there.
    status_t CopyLinearToGraphic(const android::C2BufferPack& input,
        const std::shared_ptr<android::C2BlockAllocator>& allocator,
        std::shared_ptr<android::C2Buffer>* out_buffer);

    void DoWork(std::unique_ptr<android::C2Work>&& work);

private:
    Type type_;

    MfxCmdQueue cmd_queue_;
};
