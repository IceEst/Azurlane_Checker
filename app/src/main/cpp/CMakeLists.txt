cmake_minimum_required(VERSION 3.22.1)
project("fakeio")

# 1. 核心修复：禁用在 Android ARM64 下有汇编语法错误的闭包桥接模块
# 这是解决 "invalid symbol kind for ADRP relocation" 的唯一有效方法
set(DOBBY_BUILD_CLOSURE_TRAMPOLINE OFF CACHE BOOL "" FORCE)

# 2. 建议：强制 Dobby 编译为静态库，以减少 native 层的符号冲突
set(DOBBY_BUILD_SHARED OFF CACHE BOOL "" FORCE)

set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/dobby)

# 设置 Dobby 的头文件路径
include_directories(${DOBBY_DIR}/include)

# 引入 Dobby 子项目（应用上面设置的选项）
add_subdirectory(${DOBBY_DIR} dobby_build)

# 编译项目主库
add_library(fakeio SHARED fake_io.cpp)

# 链接库：dobby (静态链接), log (Android 日志库)
target_link_libraries(fakeio dobby log)
