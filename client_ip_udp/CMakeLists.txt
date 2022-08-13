# CMakeList.txt : CMake project for Recursion project, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE CLIENT_UDP_SRCS CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE CLIENT_UDP_HDRS CONFIGURE_DEPENDS *.h)
#set(SRCS "test.cpp")
#set(HDRS "")

ADD_EXECUTABLE(ClientUDP ${CLIENT_UDP_SRCS} ${CLIENT_UDP_HDRS})

target_link_libraries(ClientUDP PRIVATE OpenSSL::SSL OpenSSL::Crypto Common)

include_directories(ClientUDP ${ROOT_INCLUDE_DIRS})