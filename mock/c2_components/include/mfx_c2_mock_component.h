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
protected:
    MfxC2MockComponent(const android::C2String name, int flags);

    MFX_CLASS_NO_COPY(MfxC2MockComponent)

public:
    static void RegisterClass(MfxC2ComponentsRegistry& registry);

protected: // android::C2Component
    status_t queue_nb(std::list<std::unique_ptr<android::C2Work>>* const items) override;

    status_t start() override;

    status_t stop() override;

protected:
    android::status_t Init() override;

private:
    void DoWork(std::unique_ptr<android::C2Work>&& work);

private:
    MfxCmdQueue cmd_queue_;
};
