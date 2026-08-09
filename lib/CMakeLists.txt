include(FetchContent)

set(HTTPLIB_REQUIRE_ZSTD OFF CACHE BOOL "" FORCE)
set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        cpp_httplib
        GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
        GIT_TAG v0.28.0
)
FetchContent_MakeAvailable(cpp_httplib)

add_library(base_http
        http/http.cpp
)
target_link_libraries(base_http PUBLIC httplib::httplib Threads::Threads)
target_include_directories(base_http PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(http_example_hello
        http/_example/hello.cpp
)
target_link_libraries(http_example_hello PRIVATE base_http)

add_executable(http_test
        ../test_run.cpp
        http/test_scenario.cpp
)
target_link_libraries(http_test PRIVATE base_http GTest::gtest)

add_test(NAME http_test COMMAND http_test)
