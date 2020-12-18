cmake_minimum_required(VERSION 3.17)
project(sender)

set(CMAKE_CXX_STANDARD 11)

add_executable(sender server.cpp)
#link_libraries(-lIphlpapi)
#link_libraries(-lwsock32)
#link_libraries(ws2_32)
if(MINGW)
    target_link_libraries(sender ws2_32)
endif()