# CMakeList.txt : CMake project for Recursion project, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE SERVER_NAT_SRCS CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE SERVER_NAT_HDRS CONFIGURE_DEPENDS *.h)
#set(SRCS "test.cpp")
#set(HDRS "")

ADD_EXECUTABLE(ServerNAT ${SERVER_NAT_SRCS} ${SERVER_NAT_HDRS})

target_link_libraries(ServerNAT OpenSSL::SSL OpenSSL::Crypto Common)