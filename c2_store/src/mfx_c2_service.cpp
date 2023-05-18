// Copyright (c) 2017-2022 Intel Corporation
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


/*
 * Copyright 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// The contents of this file was copied
// from AOSP hardware/google/av/codec2/hidl/services/vendor.cpp
// and modified then.

//#define LOG_NDEBUG 0
#define LOG_TAG "hardware.intel.media.c2@1.0-service"

#include "mfx_c2_store.h"

#include <codec2/hidl/1.0/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>
#include <binder/ProcessState.h>
#include <minijail.h>
#include <dlfcn.h>

#include <C2Component.h>

// This is created by module "codec2.vendor.base.policy". This can be modified.
static constexpr char kBaseSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/android.hardware.media.c2@1.0-vendor.policy";

// Additional device-specific seccomp permissions can be added in this file.
static constexpr char kExtSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/android.hardware.media.c2@1.0-vendor.ext.policy";

// Create and register IComponentStore service.
void RegisterC2Service()
{
    using namespace ::android::hardware::media::c2::V1_0;
    android::sp<IComponentStore> store;

    ALOGD("Instantiating MFX IComponentStore service...");

    c2_status_t status = C2_OK;
    std::shared_ptr<C2ComponentStore> c2_store(MfxC2ComponentStore::Create(&status));
    if (c2_store) {
        store = new utils::ComponentStore(c2_store);
    } else {
        ALOGD("Creation MFX IComponentStore failed with status: %d", (int)status);
    }

    if (!store) {
        ALOGE("Cannot create Codec2's IComponentStore service.");
    } else {
        if (store->registerAsService("default") != android::OK) {
            ALOGE("Cannot register Codec2's "
                    "IComponentStore service.");
        } else {
            ALOGI("Codec2's IComponentStore service created.");
        }
    }
}


int main(int /* argc */, char** /* argv */) {
    ALOGD("hardware.intel.media.c2@1.0-service starting...");

    signal(SIGPIPE, SIG_IGN);
    android::SetUpMinijail(kBaseSeccompPolicyPath, kExtSeccompPolicyPath);

    // vndbinder is needed by BufferQueue.
    android::ProcessState::initWithDriver("/dev/vndbinder");
    android::ProcessState::self()->startThreadPool();

    // Extra threads may be needed to handle a stacked IPC sequence that
    // contains alternating binder and hwbinder calls. (See b/35283480.)
    android::hardware::configureRpcThreadpool(8, true /* callerWillJoin */);

    RegisterC2Service();

    android::hardware::joinRpcThreadpool();
    return 0;
}
