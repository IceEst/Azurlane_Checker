cmake_minimum_required(VERSION 3.22.1)
project("fakeio")

# 1. 设置 Dobby 源码路径 (Action 脚本会自动下载到这里)
set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/dobby)

# 2. 包含 Dobby 的头文件
include_directories(${DOBBY_DIR}/include)

# 3. 将 Dobby 加入编译
add_subdirectory(${DOBBY_DIR} dobby_build)

# 4. 编译我们自己的 Hook 库
add_library(fakeio SHARED fake_io.cpp)

# 5. 链接必要的库：log 是安卓日志库，dobby 是核心 hook 库
target_link_libraries(fakeio dobby log)
