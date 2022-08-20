# CMakeList.txt : CMake project for Recursion project, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE CLIENT_NAT_SRCS CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE CLIENT_NAT_HDRS CONFIGURE_DEPENDS *.h)
#set(SRCS "test.cpp")
#set(HDRS "")

ADD_EXECUTABLE(ClientNAT ${CLIENT_NAT_SRCS} ${CLIENT_NAT_HDRS})

target_link_libraries(ClientNAT PRIVATE OpenSSL::SSL OpenSSL::Crypto Common)

include_directories(ClientNAT ${ROOT_INCLUDE_DIRS})