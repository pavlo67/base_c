# test

Minimal dependency-free test helper used by the project tests. It provides the small subset of GTest-style `TEST`, `ASSERT_*`, `EXPECT_*`, and `ASSERT_NEAR` macros currently needed by the codebase. `base/test_run.cpp` supplies the common test runner; CMake registers the resulting executables with CTest.
