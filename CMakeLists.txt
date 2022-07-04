# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.8)

project ("NATBuster")
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

# Client
add_subdirectory ("client")
add_subdirectory ("common")
add_subdirectory ("server")
add_subdirectory ("test")