cmake_minimum_required(VERSION 3.20)
project(akbench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
enable_testing()

add_subdirectory(akbench)
