cmake_minimum_required(VERSION 3.22.1)
project("fakeio")

set(DOBBY_DIR ${CMAKE_SOURCE_DIR}/dobby)
include_directories(${DOBBY_DIR}/include)
add_subdirectory(${DOBBY_DIR} dobby_build)

add_library(fakeio SHARED fake_io.cpp)
target_link_libraries(fakeio dobby log)
