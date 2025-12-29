cmake_minimum_required(VERSION 3.22.1)
project("fakeio")

# 修复：禁用掉在 Android 上有汇编兼容问题的 Closure Trampoline 功能
# 你的项目目前不需要这个功能，禁用它可以绕过编译错误
set(DOBBY_BUILD_CLOSURE_TRAMPOLINE OFF CACHE BOOL "" FORCE)

set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/dobby)
include_directories(${DOBBY_DIR}/include)
add_subdirectory(${DOBBY_DIR} dobby_build)

add_library(fakeio SHARED fake_io.cpp)
target_link_libraries(fakeio dobby log)
