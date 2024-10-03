cmake_minimum_required(VERSION 3.12)

project(test_components)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_BUILD_TYPE Debug)

include_directories(../include)

# 查找测试文件
file(GLOB_RECURSE TEST_SRC_LIST "code/test_log.cpp")
# 查找项目实现文件
file(GLOB_RECURSE LOG_SOURCES "../src/log/*.cpp")
file(GLOB_RECURSE BUFFER_SOURCES "../src/buffer/*.cpp")
set(SRC_LIST ${LOG_SOURCES} ${BUFFER_SOURCES})

# 设置测试二进制文件的输出路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)

add_executable(tests ${TEST_SRC_LIST} ${SRC_LIST})

# 链接GTEST
find_package(GTest REQUIRED)
target_link_libraries(tests GTest::GTest GTest::Main pthread)