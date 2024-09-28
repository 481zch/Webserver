cmake_minimum_required(VERSION 3.12)

project(webserver)

set(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED TRUE)

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

INCLUDE_DIRECTORIES(include)

FILE(GLOB_RECURSE SRC_LIST "src/*.cpp")

add_executable(webserver ${SRC_LIST})

target_link_libraries(webserver pthread)
