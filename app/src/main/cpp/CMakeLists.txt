cmake_minimum_required(VERSION 3.22.1)
project("fakeio")

# 1. 核心修复：使用正确的变量名禁用闭包跳板模块
# 尝试使用两种可能的变量名来彻底禁用
set(DOBBY_BuildClosureTrampoline OFF CACHE BOOL "" FORCE)
set(BuildClosureTrampoline OFF CACHE BOOL "" FORCE)

# 同时建议禁用生成汇编跳板（如果适用）
set(DOBBY_GENERATE_SHARED OFF CACHE BOOL "" FORCE)

# 2. 建议：禁用 Dobby 的示例程序和测试，避免不必要的编译错误
set(BuildExamples OFF CACHE BOOL "" FORCE)

# 3. 强制 Dobby 编译为静态库
set(BuildShared OFF CACHE BOOL "" FORCE)

set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/dobby)

# 设置 Dobby 的头文件路径
include_directories(${DOBBY_DIR}/include)

# 引入 Dobby 子项目
add_subdirectory(${DOBBY_DIR} dobby_build)

# 编译插件主库
add_library(fakeio SHARED fake_io.cpp)

# 链接库
target_link_libraries(fakeio dobby log)
