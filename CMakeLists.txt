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
    # find_package(fmt REQUIRED)

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
        FetchContent_MakeAvailable(mongoose_source)
        # FetchContent_Populate(mongoose_source) // deprecated method
    endif()

    if(NOT TARGET mongoose)
        add_library(mongoose STATIC ${mongoose_source_SOURCE_DIR}/mongoose.c
                lib/server/server.cpp)
        set_source_files_properties(${mongoose_source_SOURCE_DIR}/mongoose.c PROPERTIES LANGUAGE CXX)
        target_include_directories(mongoose PUBLIC ${mongoose_source_SOURCE_DIR})
    endif()

endif()

set(BASE_GPIO_PLATFORM "AUTO" CACHE STRING "GPIO implementation: AUTO, RPI4, RPI5, DESKTOP")
set_property(CACHE BASE_GPIO_PLATFORM PROPERTY STRINGS AUTO RPI4 RPI5 DESKTOP)
set(SYSTEM "${BASE_GPIO_PLATFORM}")
if(SYSTEM STREQUAL "AUTO")
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|aarch64|ARM64)")
        if(CMAKE_CROSSCOMPILING OR NOT EXISTS "/proc/device-tree/model")
            message(FATAL_ERROR "Set BASE_GPIO_PLATFORM to RPI4 or RPI5 for this ARM build")
        endif()
        file(READ "/proc/device-tree/model" GPIO_BOARD_MODEL)
        if(GPIO_BOARD_MODEL MATCHES "Raspberry Pi 5")
            set(SYSTEM "RPI5")
        elseif(GPIO_BOARD_MODEL MATCHES "Raspberry Pi 4")
            set(SYSTEM "RPI4")
        else()
            message(FATAL_ERROR "Unsupported board; select BASE_GPIO_PLATFORM explicitly")
        endif()
    else()
        set(SYSTEM "DESKTOP")
    endif()
endif()

if(SYSTEM STREQUAL "RPI4")
    find_library(PIGPIO_LIBRARY NAMES pigpio REQUIRED)
    find_path(PIGPIO_INCLUDE_DIR pigpio.h REQUIRED)
    set(GPIO_INCLUDE_DIR ${PIGPIO_INCLUDE_DIR})
    set(RPI_LIBS ${PIGPIO_LIBRARY} rt)
elseif(SYSTEM STREQUAL "RPI5")
    find_library(LGPIO_LIBRARY NAMES lgpio REQUIRED)
    find_path(LGPIO_INCLUDE_DIR lgpio.h REQUIRED)
    set(GPIO_INCLUDE_DIR ${LGPIO_INCLUDE_DIR})
    set(RPI_LIBS ${LGPIO_LIBRARY})
elseif(NOT SYSTEM STREQUAL "DESKTOP")
    message(FATAL_ERROR "Unknown BASE_GPIO_PLATFORM: ${SYSTEM}")
endif()
message(STATUS "GPIO platform: ${SYSTEM}")
add_compile_definitions(SYSTEM_IS_${SYSTEM}=1)

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
