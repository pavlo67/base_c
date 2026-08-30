cmake_minimum_required(VERSION 3.16)
project(base_cpp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CTEST_PARALLEL_LEVEL 1)
enable_testing()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)


# external libs --------------------------------------------------------

if(NOT PAN_TILT_ROOT)

    message("USING LOCAL BASE DEPENDENCIES")

    include(FetchContent)
    FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0
    )
    FetchContent_MakeAvailable(googletest)

    find_package(yaml-cpp)
    find_package(jsoncpp)
    # add_library(jsoncpp_lib STATIC IMPORTED)
    #if (SYSTEM STREQUAL "DESKTOP")
    #    set_target_properties(jsoncpp_lib PROPERTIES IMPORTED_LOCATION "/usr/local/lib/x86_64-linux-gnu/libjsoncpp.a")
    #else()
    #    set_target_properties(jsoncpp_lib PROPERTIES IMPORTED_LOCATION "/usr/local/lib/aarch64-linux-gnu/libjsoncpp.a")
    #endif()


    set(HTTPLIB_REQUIRE_ZSTD OFF CACHE BOOL "" FORCE)
    set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
            cpp_httplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            GIT_TAG v0.28.0
    )
    FetchContent_MakeAvailable(cpp_httplib)

    FetchContent_Declare(
            mongoose_source
            GIT_REPOSITORY https://github.com/cesanta/mongoose.git
            GIT_TAG 7.22
    )
    FetchContent_GetProperties(mongoose_source)
    if(NOT mongoose_source_POPULATED)
        FetchContent_Populate(mongoose_source)
    endif()

    if(NOT TARGET mongoose)
        add_library(mongoose STATIC ${mongoose_source_SOURCE_DIR}/mongoose.c)
        set_source_files_properties(${mongoose_source_SOURCE_DIR}/mongoose.c PROPERTIES LANGUAGE CXX)
        target_include_directories(mongoose PUBLIC ${mongoose_source_SOURCE_DIR})
    endif()

    # find_package(fmt REQUIRED)
    # find_package(yaml-cpp)
    # pkg_check_modules(LZ4 REQUIRED liblz4)

endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm" OR CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    unset(PIGIO CACHE)
    find_library(PIGIO pigpio NO_CACHE)

    if (PIGIO STREQUAL "PIGIO-NOTFOUND")
        message("RPI5")
        add_compile_definitions(SYSTEM_IS_RPI5=1)
        set(SYSTEM "RPI5")

    else()
        message("RPI4")
        add_compile_definitions(SYSTEM_IS_RPI4=1)
        set(SYSTEM "RPI4")
        list(APPEND RPI_LIBS ${PIGIO} rt)
    endif()

else()
    message("DESKTOP")
    add_compile_definitions(SYSTEM_IS_DESKTOP=1)
    set(SYSTEM "DESKTOP")

endif()

option(USE_CAMERA "Find and use libcamera" OFF)

if(USE_CAMERA)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CAMERA REQUIRED libcamera)               # sudo apt install libcamera-dev
    find_library(LIBCAMERA_LIBRARY libcamera.so REQUIRED)
    find_library(LIBCAMERA_BASE_LIBRARY libcamera-base.so REQUIRED)
    set(LIBCAMERA_LIBS "${LIBCAMERA_LIBRARY}" "${LIBCAMERA_BASE_LIBRARY}")
    include_directories(${CAMERA_INCLUDE_DIRS})
endif()

#find_package(SDL2 REQUIRED)                     # sudo apt install libsdl2-dev libsdl2-2.0-0
#include_directories(${SDL2_INCLUDE_DIRS})

#find_package(OpenCV)  # 4.12.0
#include_directories(${OpenCV_INCLUDE_DIRS})
#message(STATUS "OpenCV include dirs: ${OpenCV_INCLUDE_DIRS}")

# /external libs -------------------------------------------------------

# project libs ---------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/_bin)

include_directories(./)
add_subdirectory(hardware/)
add_subdirectory(lib/)


# hello ----------------------------------------------------------------

# The top-level project defines its own `hello` target. Keep this small
# standalone executable only when `base` is opened as the root project.
if(NOT TARGET hello)
    add_executable(hello
            apps/hello.cpp
    )
    target_link_libraries(hello PRIVATE Threads::Threads)
endif()
