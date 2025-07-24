#! /bin/bash

set -eux -o pipefail

git submodule update --init --recursive
cmake -S . \
    -B build \
    -D CMAKE_BUILD_TYPE=RelWithDebInfo \
    -D CMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build -j
