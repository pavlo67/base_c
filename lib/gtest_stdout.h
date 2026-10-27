#pragma once

#include <gtest/gtest.h>

#include <string>
#include <type_traits>
#include <utility>

namespace test_support {

    template <typename Result>
    struct CapturedStdout {
        Result result_;
        std::string output_;
    };

    template <typename Action>
    auto captureStdout(Action&& action) {
        using Result = std::invoke_result_t<Action>;
        testing::internal::CaptureStdout();
        try {
            Result result = std::forward<Action>(action)();
            return CapturedStdout<Result>{
                .result_ = std::move(result),
                .output_ = testing::internal::GetCapturedStdout()
            };
        } catch (...) {
            static_cast<void>(testing::internal::GetCapturedStdout());
            throw;
        }
    }

}
