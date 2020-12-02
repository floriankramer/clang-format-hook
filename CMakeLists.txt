cmake_minimum_required(VERSION 3.11)
project(clang-format-hook)

set(CMAKE_CXX_STANDARD 20)

add_executable(clang-format-hook src/clang-format-hook.cpp)
