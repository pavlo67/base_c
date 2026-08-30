add_library(base_http
        server/cpp-httplib/http.cpp
)
target_link_libraries(base_http PUBLIC httplib::httplib Threads::Threads)
target_include_directories(base_http PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(http_example_hello
        server/cpp-httplib/_example/hello.cpp
)
target_link_libraries(http_example_hello PRIVATE base_http)

add_executable(http_test
        ../test_run.cpp
        server/cpp-httplib/http_test.cpp
)
target_link_libraries(http_test PRIVATE base_http GTest::gtest)

add_test(NAME http_test COMMAND http_test)

add_library(base_server
        server/mongoose/mong.cpp
)
target_link_libraries(base_server PUBLIC mongoose Threads::Threads)
target_include_directories(base_server PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(server_example_hello
        server/mongoose/_example/hello.cpp
)
target_link_libraries(server_example_hello PRIVATE base_server)

add_executable(server_test
        ../test_run.cpp
        server/mongoose/mong_test.cpp
)
target_link_libraries(server_test PRIVATE base_server GTest::gtest)

add_test(NAME server_test COMMAND server_test)
