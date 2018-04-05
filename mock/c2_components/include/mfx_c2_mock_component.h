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

namespace android {

enum C2ParamIndexKindMock : uint32_t {
    kParamIndexProducerMemoryType = C2Param::TYPE_INDEX_VENDOR_START,
};

typedef C2PortParam<C2Setting, C2Uint64Value, kParamIndexProducerMemoryType>::output C2ProducerMemoryType;

} // namespace android

class MfxC2MockComponent : public MfxC2Component
{
public:
    enum Type {
        Encoder,
        Decoder
    };
protected:
    MfxC2MockComponent(const C2String name, int flags, Type type);

    MFX_CLASS_NO_COPY(MfxC2MockComponent)

public:
    static void RegisterClass(MfxC2ComponentsRegistry& registry);

protected: // C2ComponentInterface
    c2_status_t config_vb(
        const std::vector<C2Param*> &params,
        c2_blocking_t mayBlock,
        std::vector<std::unique_ptr<C2SettingResult>>* const failures) override;

protected: // C2Component
    c2_status_t queue_nb(std::list<std::unique_ptr<C2Work>>* const items) override;

protected:
    c2_status_t Init() override;

    c2_status_t DoStart() override;

    c2_status_t DoStop() override;

private:
    // Allocates linear block of the length as input and copies input there.
    c2_status_t CopyGraphicToLinear(const C2FrameData& input,
        const std::shared_ptr<C2BlockPool>& allocator,
        std::shared_ptr<C2Buffer>* out_buffer);
    // Allocates graphic block of the length as input and copies input there.
    c2_status_t CopyLinearToGraphic(const C2FrameData& input,
        const std::shared_ptr<C2BlockPool>& allocator,
        std::shared_ptr<C2Buffer>* out_buffer);

    void DoWork(std::unique_ptr<C2Work>&& work);

private:
    Type type_;

    MfxCmdQueue cmd_queue_;

    C2MemoryUsage::Producer producer_memory_type_ { C2MemoryUsage::CPU_WRITE };

    std::shared_ptr<C2BlockPool> c2_allocator_;
};
