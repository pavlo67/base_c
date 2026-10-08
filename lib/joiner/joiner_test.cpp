#include "joiner.h"

#include <cassert>

#include <gtest/gtest.h>

#include "joiner.h"

int f1(int x, double y) {
    return x + static_cast<int>(y);
}

bool f2(const std::string& s) {
    return !s.empty();
}

using F1 = int (*)(int, double);
using F2 = bool (*)(const std::string&);

TEST(JoinerTest, joiner_test) {
    Joiner joiner;

    // Реєстрація
    joiner.Set("I1", &f1);
    joiner.Set("I2", &f2);

    // I1: отримання за типом функції
    auto p1 = joiner.Get<decltype(&f1)>("I1");
    ASSERT_NE(p1, nullptr);
    ASSERT_EQ(p1, &f1);
    ASSERT_EQ(p1(10, 2.5), 12);

    // I1: отримання за іменованим типом
    auto p1_alias = joiner.Get<F1>("I1");
    ASSERT_NE(p1_alias, nullptr);
    ASSERT_EQ(p1_alias, &f1);

    // I2: правильна сигнатура
    auto p2 = joiner.Get<decltype(&f2)>("I2");
    ASSERT_NE(p2, nullptr);
    ASSERT_EQ(p2, &f2);
    ASSERT_TRUE(p2("hello"));

    // I1: неправильна сигнатура
    ASSERT_EQ(joiner.Get<decltype(&f2)>("I1"), nullptr);

    // I1: неправильна сигнатура через alias
    ASSERT_EQ(joiner.Get<F2>("I1"), nullptr);

    // I3: неіснуючий ключ
    ASSERT_EQ(joiner.Get<F1>("I3"), nullptr);
}