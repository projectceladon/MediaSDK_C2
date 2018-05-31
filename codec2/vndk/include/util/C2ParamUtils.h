/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef C2UTILS_PARAM_UTILS_H_
#define C2UTILS_PARAM_UTILS_H_

#include <C2Param.h>
#include <util/_C2MacroUtils.h>

#include <utility>
#include <vector>

/** \file
 * Utilities for parameter handling to be used by Codec2 implementations.
 */

/// \cond INTERNAL

/* ---------------------------- UTILITIES FOR ENUMERATION REFLECTION ---------------------------- */

/**
 * Utility class that allows ignoring enum value assignment (e.g. both '(_C2EnumConst)kValue = x'
 * and '(_C2EnumConst)kValue' will eval to kValue.
 */
template<typename T>
class _C2EnumConst {
public:
    // implicit conversion from T
    inline _C2EnumConst(T value) : _mValue(value) {}
    // implicit conversion to T
    inline operator T() { return _mValue; }
    // implicit conversion to C2Value::Primitive
    inline operator C2Value::Primitive() { return (T)_mValue; }
    // ignore assignment and return T here to avoid implicit conversion to T later
    inline T &operator =(T value __unused) { return _mValue; }
private:
    T _mValue;
};

/// mapper to get name of enum
/// \note this will contain any initialization, which we will remove when converting to lower-case
#define _C2_GET_ENUM_NAME(x, y) #x
/// mapper to get value of enum
#define _C2_GET_ENUM_VALUE(x, type) (_C2EnumConst<type>)x

/// \endcond

#ifdef __C2_GENERATE_GLOBAL_VARS__

#undef DEFINE_C2_ENUM_VALUE_AUTO_HELPER
#define DEFINE_C2_ENUM_VALUE_AUTO_HELPER(name, type, prefix, ...) \
template<> C2FieldDescriptor::NamedValuesType C2FieldDescriptor::namedValuesFor(const name &r __unused) { \
    return C2ParamUtils::sanitizeEnumValues( \
            std::vector<C2Value::Primitive> { _C2_MAP(_C2_GET_ENUM_VALUE, type, __VA_ARGS__) }, \
            { _C2_MAP(_C2_GET_ENUM_NAME, type, __VA_ARGS__) }, \
            prefix); \
}

#undef DEFINE_C2_ENUM_VALUE_CUSTOM_HELPER
#define DEFINE_C2_ENUM_VALUE_CUSTOM_HELPER(name, type, names, ...) \
template<> C2FieldDescriptor::NamedValuesType C2FieldDescriptor::namedValuesFor(const name &r __unused) { \
    return C2ParamUtils::customEnumValues( \
            std::vector<std::pair<C2StringLiteral, name>> names); \
}

#endif

class C2ParamUtils {
    static C2String camelCaseToDashed(C2String name);

    static std::vector<C2String> sanitizeEnumValueNames(
            const std::vector<C2StringLiteral> names,
            C2StringLiteral _prefix = NULL);

    friend class C2UtilTest_ParamUtilsTest_Test;

public:
    static std::vector<C2String> parseEnumValuesFromString(C2StringLiteral value);

    template<typename T>
    static C2FieldDescriptor::NamedValuesType sanitizeEnumValues(
            std::vector<T> values,
            std::vector<C2StringLiteral> names,
            C2StringLiteral prefix = NULL) {
        C2FieldDescriptor::NamedValuesType namedValues;
        std::vector<C2String> sanitizedNames = sanitizeEnumValueNames(names, prefix);
        for (size_t i = 0; i < values.size() && i < sanitizedNames.size(); ++i) {
            namedValues.emplace_back(sanitizedNames[i], values[i]);
        }
        return namedValues;
    }

    template<typename E>
    static C2FieldDescriptor::NamedValuesType customEnumValues(
            std::vector<std::pair<C2StringLiteral, E>> items) {
        C2FieldDescriptor::NamedValuesType namedValues;
        for (auto &item : items) {
            namedValues.emplace_back(item.first, item.second);
        }
        return namedValues;
    }

    /// safe(r) parsing from parameter blob
    static
    C2Param *ParseFirst(const uint8_t *blob, size_t size);
};

/* ---------------------------- UTILITIES FOR PARAMETER REFLECTION ---------------------------- */

/* ======================== UTILITY TEMPLATES FOR PARAMETER REFLECTION ======================== */

template<typename... Params>
class C2_HIDE _C2Tuple { };

/* ---------------------------- UTILITIES FOR ENUMERATION REFLECTION ---------------------------- */

#endif  // C2UTILS_PARAM_UTILS_H_

