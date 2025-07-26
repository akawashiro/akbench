#! /bin/bash

set -eux -o pipefail

SCR_DIR=$(git rev-parse --show-toplevel)/ipc-bench
BUILD_DIR=$(git rev-parse --show-toplevel)/build

git submodule update --init --recursive
cmake -S "$SCR_DIR" \
    -B "$BUILD_DIR" \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
