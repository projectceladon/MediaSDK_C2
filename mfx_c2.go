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

func includeDirs(ctx android.LoadHookContext) []string {
    var include_dirs []string

    if ctx.AConfig().PlatformSdkVersion().FinalOrFutureInt() >= 34 {
        include_dirs = append(include_dirs, "frameworks/av/media/module/foundation/include")
        include_dirs = append(include_dirs, "frameworks/av/media/module/bufferpool/2.0/include")
    } else {
        include_dirs = append(include_dirs, "frameworks/av/media/libstagefright/foundation/include")
        include_dirs = append(include_dirs, "frameworks/av/media/bufferpool/2.0/include")
    }

    return include_dirs
}

func mfxCcDefaults(ctx android.LoadHookContext) {
    type props struct {
        Cflags []string
        include_dirs []string
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

    var dirs = includeDirs(ctx)
    for _, path := range dirs {
        p.include_dirs = append(p.include_dirs, path)
    }

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
