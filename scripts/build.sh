#! /bin/bash

set -eux -o pipefail

SCR_DIR=$(git rev-parse --show-toplevel)
BUILD_DIR=$(git rev-parse --show-toplevel)/build

git submodule update --init --recursive
cmake -S "$SCR_DIR" \
    -B "$BUILD_DIR" \
    -D CMAKE_CXX_COMPILER=clang++ \
    -D CMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -D AKBENCH_ENABLE_MPI=ON \
    -D CMAKE_BUILD_WITH_INSTALL_RPATH=ON \
    -G Ninja
cmake --build build -j
ctest --test-dir build
ln -sf "$BUILD_DIR"/compile_commands.json "$SCR_DIR"/compile_commands.json
