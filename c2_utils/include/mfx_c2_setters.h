// Copyright (c) 2018-2021 Intel Corporation
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

#include <C2Config.h>
#include "util/C2InterfaceHelper.h"

using namespace android;

template<typename T>
using C2P = C2InterfaceHelper::Param<T>;

template<typename T, typename ...Args>
std::shared_ptr<T> AllocSharedString(const Args(&... args), const char *str) {
    size_t len = strlen(str) + 1;
    std::shared_ptr<T> ret = T::AllocShared(len, args...);
    strcpy(ret->m.value, str);
    return ret;
}

template<typename T, typename ...Args>
std::shared_ptr<T> AllocSharedString(const Args(&... args), const std::string &str) {
    std::shared_ptr<T> ret = T::AllocShared(str.length() + 1, args...);
    strcpy(ret->m.value, str.c_str());
    return ret;
}

template <typename T>
struct Setter {
    typedef typename std::remove_reference<T>::type type;

    static C2R NonStrictValueWithNoDeps(
            bool mayBlock, C2InterfaceHelper::C2P<type> &me) {
        (void)mayBlock;
        return me.F(me.v.value).validatePossible(me.v.value);
    }

    static C2R NonStrictValuesWithNoDeps(
            bool mayBlock, C2InterfaceHelper::C2P<type> &me) {
        (void)mayBlock;
        C2R res = C2R::Ok();
        for (size_t ix = 0; ix < me.v.flexCount(); ++ix) {
            res.plus(me.F(me.v.m.values[ix]).validatePossible(me.v.m.values[ix]));
        }
        return res;
    }

    static C2R StrictValueWithNoDeps(
            bool mayBlock,
            const C2InterfaceHelper::C2P<type> &old,
            C2InterfaceHelper::C2P<type> &me) {
        (void)mayBlock;
        if (!me.F(me.v.value).supportsNow(me.v.value)) {
            me.set().value = old.v.value;
        }
        return me.F(me.v.value).validatePossible(me.v.value);
    }
};