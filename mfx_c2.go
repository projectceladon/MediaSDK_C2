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
// https://android.googlesource.com/platform/build/soong/+/master/cc/cc.go

package mfx_c2

import (
    "android/soong/android"
    "android/soong/cc"

    "regexp"
    "strings"
)

func matchWhole(patterns string, s string) bool {
    patterns = strings.Replace(patterns, ".", "\\.", -1)
    patterns = strings.Replace(patterns, "*", ".*", -1)
    res, _ := regexp.MatchString("^(" + patterns + ")$", s)
    return res
}

func mfxCcDefaults(ctx android.LoadHookContext) {
    type props struct {
        Cflags []string
    }

    p := &props{}

    var androidVersion string
    platformVersion := ctx.AConfig().PlatformVersionName()

    switch {
    case matchWhole("9|9.*|P", platformVersion):
        androidVersion = "MFX_P"
    case matchWhole("8.*|O", platformVersion):
        if matchWhole("8.0.*", platformVersion) {
            androidVersion = "MFX_O"
        } else {
            androidVersion = "MFX_O_MR1"
        }
    case matchWhole("7.*|N", platformVersion):
        androidVersion = "MFX_N"
    case matchWhole("6.*|M", platformVersion):
        androidVersion = "MFX_MM"
    case matchWhole("5.*|L", platformVersion):
        androidVersion = "MFX_LD"
    }

    p.Cflags = append(p.Cflags, "-DMFX_ANDROID_VERSION=" + androidVersion)
    p.Cflags = append(p.Cflags, "-DMFX_ANDROID_PLATFORM=" + ctx.AConfig().DeviceName())
    ctx.AppendProperties(p)
}

func init() {
    android.RegisterModuleType("mfx_cc_defaults", mfxCcDefaultsFactory)
}

func mfxCcDefaultsFactory() android.Module {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, mfxCcDefaults)
    return module
}
