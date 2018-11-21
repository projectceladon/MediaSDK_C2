/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

// Copyright 2016 Google Inc. All rights reserved.
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

// Some parts of this file were copied from:
// https://android.googlesource.com/platform/build/soong/+/master/cc/library.go

package c2_service

import (
    "android/soong/android"
    "android/soong/cc"

    "github.com/google/blueprint/proptools"
)

func c2ServiceBinaryDefaults(ctx android.LoadHookContext) {
    type props struct {
        Target struct {
            Android struct {
                Enabled *bool
            }
        }
    }

    p := &props{}
    ret := ctx.AConfig().Getenv("BUILD_C2_SERVICE")
    if ret == "true" {
        p.Target.Android.Enabled = proptools.BoolPtr(true)
    } else {
        p.Target.Android.Enabled = proptools.BoolPtr(false)
    }

    ctx.AppendProperties(p)
}

func init() {
    android.RegisterModuleType("cc_binary_c2_service", c2ServiceBinary)
}

func c2ServiceBinary() android.Module {
    m, _ := cc.NewBinary(android.HostAndDeviceSupported)
    module := m.Init()

    android.AddLoadHook(module, c2ServiceBinaryDefaults)

    return module
}
