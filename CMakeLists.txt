cmake_minimum_required(VERSION 3.12)

project(webserver)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_BUILD_TYPE Debug)


SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

INCLUDE_DIRECTORIES(src)

FILE(GLOB_RECURSE SRC_LIST "src/*.cpp")

add_executable(webserver ${SRC_LIST})

target_link_libraries(webserver pthread mysqlclient hiredis ssl crypto)
