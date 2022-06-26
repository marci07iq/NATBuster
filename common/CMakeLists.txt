# CMakeList.txt : CMake project for Recursion project, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE COMMON_SRCS CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE COMMON_HDRS CONFIGURE_DEPENDS *.h)
#set(SRCS "test.cpp")
#set(HDRS "")

add_library(Common STATIC ${COMMON_SRCS} ${COMMON_HDRS})

include_directories(Common ${ROOT_INCLUDE_DIRS})