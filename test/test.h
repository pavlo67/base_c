#pragma once

#include <cmath>
#include <exception>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace test {
struct Failure : std::exception {};
struct Case { const char* suite; const char* name; void (*fn)(); };
inline std::vector<Case>& cases() { static std::vector<Case> v; return v; }
inline int& failures() { static int n = 0; return n; }
struct Register { Register(const char* s, const char* n, void (*f)()) { cases().push_back({s,n,f}); } };
inline void fail(const char* file, int line, const std::string& msg, bool fatal) {
    ++failures(); std::cerr << file << ':' << line << ": " << msg << '\n';
    if (fatal) throw Failure{};
}
template<class A, class B> inline std::string values(const A& a, const B& b) { std::ostringstream s; s << "values: " << a << " vs " << b; return s.str(); }
inline int runAll() {
    int failedCases = 0;
    for (const auto& c : cases()) {
        const int before = failures();
        try { c.fn(); } catch (const Failure&) {} catch (const std::exception& e) { fail(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what(), false); } catch (...) { fail(__FILE__, __LINE__, "unexpected exception", false); }
        if (failures() != before) { ++failedCases; std::cerr << "[FAILED] " << c.suite << '.' << c.name << '\n'; }
        else std::cout << "[OK] " << c.suite << '.' << c.name << '\n';
    }
    std::cout << cases().size() << " test(s), " << failedCases << " failed\n";
    return failedCases ? 1 : 0;
}
}

#define TEST(SUITE, NAME) static void SUITE##_##NAME(); static ::test::Register reg_##SUITE##_##NAME(#SUITE, #NAME, &SUITE##_##NAME); static void SUITE##_##NAME()
#define TEST_CMP(A,B,OP,TXT,FATAL) do { const auto& _a=(A); const auto& _b=(B); if (!(_a OP _b)) ::test::fail(__FILE__,__LINE__, std::string(TXT " failed: ") + #A " " #OP " " #B + " (" + ::test::values(_a,_b) + ")", FATAL); } while(0)
#define ASSERT_EQ(A,B) TEST_CMP(A,B,==,"ASSERT_EQ",true)
#define ASSERT_NE(A,B) TEST_CMP(A,B,!=,"ASSERT_NE",true)
#define ASSERT_LT(A,B) TEST_CMP(A,B,<,"ASSERT_LT",true)
#define ASSERT_LE(A,B) TEST_CMP(A,B,<=,"ASSERT_LE",true)
#define ASSERT_GT(A,B) TEST_CMP(A,B,>,"ASSERT_GT",true)
#define ASSERT_GE(A,B) TEST_CMP(A,B,>=,"ASSERT_GE",true)
#define EXPECT_EQ(A,B) TEST_CMP(A,B,==,"EXPECT_EQ",false)
#define EXPECT_NE(A,B) TEST_CMP(A,B,!=,"EXPECT_NE",false)
#define ASSERT_TRUE(A) do { if (!(A)) ::test::fail(__FILE__,__LINE__, "ASSERT_TRUE failed: " #A, true); } while(0)
#define EXPECT_TRUE(A) do { if (!(A)) ::test::fail(__FILE__,__LINE__, "EXPECT_TRUE failed: " #A, false); } while(0)
#define ASSERT_NEAR(A,B,E) do { const auto _a=(A); const auto _b=(B); const auto _e=(E); if (std::abs(_a-_b) > _e) ::test::fail(__FILE__,__LINE__, "ASSERT_NEAR failed: " #A ", " #B, true); } while(0)
