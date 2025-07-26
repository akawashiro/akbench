cmake_minimum_required(VERSION 3.20)
project(ipc-bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

add_subdirectory(ipc-bench)
