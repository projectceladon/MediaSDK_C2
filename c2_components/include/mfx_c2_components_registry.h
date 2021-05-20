// Copyright (c) 2017-2021 Intel Corporation
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

#include "mfx_defs.h"
#include "mfx_c2_defs.h"
#include "mfx_c2_component.h"
#include "mfx_c2_param_reflector.h"

#include <C2Component.h>

#include <map>
#include <functional>

// function declaration to make possible use this function statically too
extern "C" CreateMfxC2ComponentFunc MfxCreateC2Component;

class MfxC2ComponentsRegistry
{
private:
    MfxC2ComponentsRegistry();

    MFX_CLASS_NO_COPY(MfxC2ComponentsRegistry);

public:
    static MfxC2ComponentsRegistry& getInstance();

    c2_status_t CreateMfxC2Component(const char* name, const MfxC2Component::CreateConfig& config,
        std::shared_ptr<MfxC2ParamReflector> reflector, MfxC2Component** component);

    void RegisterMfxC2Component(const std::string& name, CreateMfxC2ComponentFunc* createFunc);

private:
    std::map<std::string, CreateMfxC2ComponentFunc*> registry_;
};
