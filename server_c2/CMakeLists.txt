﻿# CMakeList.txt : CMake project for Recursion project, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE SERVER_SRCS CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE SERVER_HDRS CONFIGURE_DEPENDS *.h)
#set(SRCS "test.cpp")
#set(HDRS "")

ADD_EXECUTABLE(ServerC2 ${SERVER_SRCS} ${SERVER_HDRS})

target_link_libraries(ServerC2 OpenSSL::SSL OpenSSL::Crypto Common)