cmake_minimum_required(VERSION 3.17)
project(receiver)

set(CMAKE_CXX_STANDARD 11)

add_executable(receiver client.cpp)
#link_libraries(-lIphlpapi)
#link_libraries(-lwsock32)
#link_libraries(ws2_32)
if(MINGW)
    target_link_libraries(receiver ws2_32)
endif()