﻿# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("NATBuster")
enable_testing()
set (CMAKE_CXX_STANDARD 20)
set(CMAKE_GENERATOR_PLATFORM=x64)

if (MSVC)
    add_compile_options(/arch:AVX2)
    add_compile_options(/bigobj)
endif()

if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} " /Ox")
endif()

find_package(OpenSSL REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)

# Client
add_subdirectory ("client")
add_subdirectory ("common")
add_subdirectory ("server_c2")
add_subdirectory ("server_ip")
add_subdirectory ("test")