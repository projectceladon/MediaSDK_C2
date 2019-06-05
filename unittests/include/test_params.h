/********************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2017-2019 Intel Corporation. All Rights Reserved.

*********************************************************************************/

#pragma once

#include <C2Param.h>

inline bool Valid(const C2Param& param)
{
    return (bool)param;
}

inline bool Equal(const C2Param& param1, const C2Param& param2)
{
    return param1 == param2;
}

inline std::string FormatParam(const C2Param& param)
{
    std::ostringstream oss;
    oss << "index: " << std::hex << param.index() << "; "
        << "size: " << std::dec << param.size() << "\n" <<
        FormatHex((const uint8_t*)&param, param.size());
    return oss.str();
}

// C2 parameters values set.
class C2ParamValues
{
    using C2Param = C2Param;
private:
    std::list<std::shared_ptr<C2Param>> expected_;
    std::vector<std::shared_ptr<C2Param>> stack_values_;
    std::vector<C2Param::Index> indices_;

public:
    template<typename ParamType>
    void Append(ParamType* param_value)
    {
        expected_.push_back(std::shared_ptr<C2Param>((C2Param*)param_value));
        stack_values_.push_back(std::shared_ptr<C2Param>((C2Param*)new ParamType()));
        indices_.push_back(ParamType::PARAM_TYPE);
    }

    template<typename ParamType>
    void AppendFlex(std::unique_ptr<ParamType>&& param_value)
    {
        // don't add to stack_values_ as flex params need dynamic allocation
        stack_values_.push_back(C2Param::Copy(*param_value));
        expected_.push_back(std::shared_ptr<C2Param>(std::move(param_value)));
        indices_.push_back(ParamType::PARAM_TYPE);
    }

    std::vector<C2Param*> GetStackPointers() const
    {
        // need this temp vector as cannot init vector<smth const> in one step
        std::vector<C2Param*> params;
        std::transform(stack_values_.begin(), stack_values_.end(), std::back_inserter(params),
            [] (const std::shared_ptr<C2Param>& p) { return p.get(); } );

        std::vector<C2Param*> res(params.begin(), params.end());
        return res;
    }

    std::vector<C2Param*> GetExpectedParams() const
    {
        // need this temp vector as cannot init vector<smth const> in one step
        std::vector<C2Param*> params;
        std::transform(expected_.begin(), expected_.end(), std::back_inserter(params),
            [] (const std::shared_ptr<C2Param>& p) { return p.get(); } );

        std::vector<C2Param*> res(params.begin(), params.end());
        return res;
    }

    std::vector<C2Param::Index> GetIndices() const
    {
        return indices_;
    }

    void CheckStackValues() const
    {
        Check<std::shared_ptr<C2Param>>(stack_values_, false);
    }
    // This method can be used for stack and heap values check both
    // as their collections are the same type.
    // But there is a significant difference when query of specific parameter failed:
    // stack value parameter should be invalidated, but heap not allocated at all.
    // To distingisugh that - bool parameter skip_invalid is intriduced: when true
    // parameters are expected to be invalid are skipped from comparison.
    template<typename C2ParamPtr>
    void Check(const std::vector<C2ParamPtr>& actual, bool skip_invalid) const
    {
        if(skip_invalid) {
            EXPECT_TRUE(expected_.size() > actual.size());
        } else {
            EXPECT_EQ(expected_.size(), actual.size());
        }

        auto actual_it = actual.begin();

        for(auto expected_it = expected_.begin(); expected_it != expected_.end(); ++expected_it) {

            const auto& expected_item = *expected_it;
            if (!Valid(*expected_item) && skip_invalid) {
                continue;
            }

            EXPECT_NE(actual_it, actual.end());

            if (actual_it != actual.end()) {

                const C2ParamPtr& actual_item = *actual_it;

                EXPECT_EQ(actual_item->index(), expected_item->index())
                    << std::hex << actual_item->index() << " instead of " << expected_item->index();

                EXPECT_EQ(Valid(*actual_item), Valid(*expected_item));

                EXPECT_EQ(actual_item->size(), expected_item->size())
                    << actual_item->size() << " instead of " << expected_item->size();

                if (Valid(*expected_item)) {
                    EXPECT_TRUE(Equal(*actual_item, *expected_item)) <<
                        "Actual:" << FormatParam(*actual_item) << "\n" <<
                        "Expected:" << FormatParam(*expected_item);
                }

                ++actual_it;
            }
        }
        EXPECT_EQ(actual_it, actual.end());
    }
};

template<typename ParamType>
static ParamType* Invalidate(ParamType* param)
{
    param->invalidate();
    return param;
}
