# CMakeList.txt : Top-level CMake project file, do global configuration
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

    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS)

    string(REGEX REPLACE "/W[1-3]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    add_compile_options (/WX)

    if (${CMAKE_BUILD_TYPE} STREQUAL "Release")
        add_compile_options(/Ox)
    endif()
else()
    add_compile_options(-Wall -Wextra -Werror)
endif()

find_package(OpenSSL REQUIRED)
#find_package(jsoncpp CONFIG REQUIRED)

# Client
add_subdirectory ("client")
add_subdirectory ("common")
add_subdirectory ("server_c2")
add_subdirectory ("server_ip")
add_subdirectory ("test")