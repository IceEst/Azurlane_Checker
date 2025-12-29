cmake_minimum_required(VERSION 3.22.1)
project("fakeio")

# 显式允许汇编文件编译
enable_language(ASM)

set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/dobby)
include_directories(${DOBBY_DIR}/include)

# 强制 Dobby 编译为静态库以减少重定位错误
set(DOBBY_BUILD_SHARED OFF CACHE BOOL "" FORCE)
add_subdirectory(${DOBBY_DIR} dobby_build)

add_library(fakeio SHARED fake_io.cpp)
target_link_libraries(fakeio dobby log)
