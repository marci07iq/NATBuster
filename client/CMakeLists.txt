# CMakeList.txt : CMake project for Recursion project, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Add source to this project's executable.
file(GLOB_RECURSE CLIENT_SRCS CONFIGURE_DEPENDS *.cpp)
file(GLOB_RECURSE CLIENT_HDRS CONFIGURE_DEPENDS *.h)
#set(SRCS "test.cpp")
#set(HDRS "")

ADD_EXECUTABLE(Client ${CLIENT_SRCS} ${CLIENT_HDRS})

target_link_libraries(Client PRIVATE OpenSSL::SSL OpenSSL::Crypto Common)
#target_link_libraries(Client PRIVATE jsoncpp_lib jsoncpp_object JsonCpp::JsonCpp)

include_directories(Client ${ROOT_INCLUDE_DIRS})