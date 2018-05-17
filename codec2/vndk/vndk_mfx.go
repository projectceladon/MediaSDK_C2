// Copyright (C) 2016 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package vndk_mfx

import (
    "android/soong/android"
    "android/soong/cc"

    "github.com/google/blueprint/proptools"
)

func vndkMfxDefaults(ctx android.LoadHookContext) {
    type props struct {
        Target struct {
            Android struct {
                Enabled *bool
            }
        }
    }

    p := &props{}
    ret := ctx.AConfig().Getenv("USE_MOCK_CODEC2")
    if ret != "true" {
        p.Target.Android.Enabled = proptools.BoolPtr(true)
    } else {
        p.Target.Android.Enabled = proptools.BoolPtr(false)
    }

    ctx.AppendProperties(p)
}

func init() {
    android.RegisterModuleType("cc_library_vndk_mfx", vndkMfxLibrary)
}

func vndkMfxLibrary() android.Module {
    m, _ := cc.NewLibrary(android.HostAndDeviceSupported)
    module := m.Init()

    android.AddLoadHook(module, vndkMfxDefaults)

    return module
}

