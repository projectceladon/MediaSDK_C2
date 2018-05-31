/*
 * Copyright (C) 2018 The Android Open Source Project
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

#ifndef SIMPLE_INTERFACE_COMMON_H_
#define SIMPLE_INTERFACE_COMMON_H_

#include <C2Component.h>
#include <util/C2InterfaceHelper.h>

namespace android {

template <typename T>
class SimpleInterface : public C2ComponentInterface {
public:
    SimpleInterface(const char *name, c2_node_id_t id, const std::shared_ptr<T> &impl)
        : mName(name),
          mId(id),
          mImpl(impl) {
    }

    ~SimpleInterface() override = default;

    // From C2ComponentInterface
    C2String getName() const override { return mName; }
    c2_node_id_t getId() const override { return mId; }
    c2_status_t query_vb(
            const std::vector<C2Param*> &stackParams,
            const std::vector<C2Param::Index> &heapParamIndices,
            c2_blocking_t mayBlock,
            std::vector<std::unique_ptr<C2Param>>* const heapParams) const override {
        return mImpl->query(stackParams, heapParamIndices, mayBlock, heapParams);
    }
    c2_status_t config_vb(
            const std::vector<C2Param*> &params,
            c2_blocking_t mayBlock,
            std::vector<std::unique_ptr<C2SettingResult>>* const failures) override {
        return mImpl->config(params, mayBlock, failures);
    }
    c2_status_t createTunnel_sm(c2_node_id_t) override { return C2_OMITTED; }
    c2_status_t releaseTunnel_sm(c2_node_id_t) override { return C2_OMITTED; }
    c2_status_t querySupportedParams_nb(
            std::vector<std::shared_ptr<C2ParamDescriptor>> * const params) const override {
        return mImpl->querySupportedParams(params);
    }
    c2_status_t querySupportedValues_vb(
            std::vector<C2FieldSupportedValuesQuery> &fields,
            c2_blocking_t mayBlock) const override {
        return mImpl->querySupportedValues(fields, mayBlock);
    }

private:
    C2String mName;
    const c2_node_id_t mId;
    const std::shared_ptr<T> mImpl;
};

template<typename T, typename ...Args>
std::shared_ptr<T> AllocSharedString(const Args(&... args), const char *str) {
    size_t len = strlen(str) + 1;
    std::shared_ptr<T> ret = T::AllocShared(len, args...);
    strcpy(ret->m.value, str);
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

}  // namespace android

#endif  // SIMPLE_INTERFACE_COMMON_H_
