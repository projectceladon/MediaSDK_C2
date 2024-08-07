// Copyright (c) 2017-2024 Intel Corporation
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "mfx_c2_component.h"
#include "mfx_c2_decoder_component.h"
#include "mfx_c2_components_registry.h"
#include "mfx_dev.h"
#include "mfx_cmd_queue.h"
#include "mfx_c2_frame_out.h"
#include "mfx_c2_bitstream_in.h"
#include "mfx_frame_pool_allocator.h"
#include "mfx_gralloc_instance.h"
#include "mfx_c2_setters.h"
#include <cutils/properties.h>

class MfxC2SecureDecoderComponent : public MfxC2DecoderComponent
{
public:
    struct DecryptionTask
    {
        mfxU16 usStatusReportFeedbackNumber {0};
        VASurfaceID surfaceId {0};
    };

public:
    MfxC2SecureDecoderComponent(const C2String name, const CreateConfig& config,
        std::shared_ptr<C2ReflectorHelper> reflector, DecoderType decoder_type);

    virtual ~MfxC2SecureDecoderComponent();

    static void RegisterClass(MfxC2ComponentsRegistry& registry);

    MFX_CLASS_NO_COPY(MfxC2SecureDecoderComponent)

protected:
    c2_status_t Init() override;

    c2_status_t Queue(std::list<std::unique_ptr<C2Work>>* const items) override;

    c2_status_t DoStart() override;

    c2_status_t DoStop(bool abort) override;

    void Decrypt(std::unique_ptr<C2Work>&& work);

    void Dowork(std::unique_ptr<C2Work>&& work, DecryptionTask&& decryptionTask, mfxU32 feedbackNumber);

    bool CheckBitstream();

    mfxStatus SubmitDecryptionTask(VAContextID contextId,
                                    mfxU16 PESPacketCounter,
                                    DecryptionTask& decryptionTask);

    mfxStatus WaitUtilDecryptionDone(DecryptionTask& decryptionTask, mfxU32 feedbackNumber);

private:
    MfxCmdQueue m_decryptionQueue;
    MFX_TRACEABLE(m_decryptionQueue);

    mfxU32 m_decrytedFeedbackNumber = 0;
    mfxU16 m_uDecrptingCount = 0;
    /* -----------------------C2Parameters--------------------------- */
    std::shared_ptr<C2SecureModeTuning> m_secureMode;
};

