// Copyright (c) 2017-2019 Intel Corporation
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

#include "mfx_dev.h"
#include "mfx_defs.h"
#include "mfx_debug.h"
#include "mfx_msdk_debug.h"

#include "mfx_dev_android.h"

#ifdef LIBVA_SUPPORT
    #include "mfx_dev_va.h"
#endif // #ifdef LIBVA_SUPPORT

#undef MFX_DEBUG_MODULE_NAME
#define MFX_DEBUG_MODULE_NAME "mfx_dev"

mfxStatus MfxDev::Create(Usage usage, std::unique_ptr<MfxDev>* device)
{
    MFX_DEBUG_TRACE_FUNC;

    std::unique_ptr<MfxDev> created_dev;

    mfxStatus sts = MFX_ERR_NONE;

#ifdef LIBVA_SUPPORT
    created_dev.reset(MFX_NEW_NO_THROW(MfxDevVa(usage)));
#else
    (void)usage;
    created_dev.reset(MFX_NEW_NO_THROW(MfxDevAndroid()));
#endif
    if(created_dev == nullptr) {
        sts = MFX_ERR_MEMORY_ALLOC;
    }

    if(created_dev != nullptr) {
        sts = created_dev->Init();
        if(sts == MFX_ERR_NONE) {
            *device = std::move(created_dev);
        }
    }

    MFX_DEBUG_TRACE_P(device->get());
    MFX_DEBUG_TRACE__mfxStatus(sts);

    return sts;
}
