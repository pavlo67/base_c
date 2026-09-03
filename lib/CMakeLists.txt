add_library(lib_server_http
        server/cpp-httplib/http.cpp
        server/server.cpp
)
target_link_libraries(lib_server_http PUBLIC httplib::httplib Threads::Threads)
target_include_directories(lib_server_http PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(server_http_hello
        server/cpp-httplib/_example/hello.cpp
        server/server.cpp
)
target_link_libraries(server_http_hello PRIVATE lib_server_http)

add_executable(server_http_test
        ../test_run.cpp
        server/cpp-httplib/http_test.cpp
        server/server.cpp
)
target_link_libraries(server_http_test PRIVATE lib_server_http) # GTest::gtest_main
add_test(NAME http_test COMMAND http_test)

add_library(lib_server_http_mngs
        server/mongoose/mngs.cpp
        server/server.cpp
)
target_link_libraries(lib_server_http_mngs PUBLIC mongoose Threads::Threads)
target_include_directories(lib_server_http_mngs PUBLIC ${CMAKE_SOURCE_DIR})

add_executable(server_http_mngs_hello
        server/mongoose/_example/hello.cpp
        server/server.cpp
)
target_link_libraries(server_http_mngs_hello PRIVATE lib_server_http_mngs)

add_executable(server_http_mngs_test
        ../test_run.cpp
        server/mongoose/mngs_test.cpp
        server/server.cpp
)
target_link_libraries(server_http_mngs_test PRIVATE lib_server_http_mngs)  # GTest::gtest_main
add_test(NAME server_http_mngs_test COMMAND server_http_mngs_test)
