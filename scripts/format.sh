#! /bin/bash

set -eux -o pipefail

PROJECT_ROOT=$(git rev-parse --show-toplevel)
cd "$PROJECT_ROOT"

for cc_file in $(git ls-files | grep -E '\.(cpp|h|hpp|c|cc|cxx)$'); do
    clang-format-18 -i "$cc_file"
done
for cmake_file in $(git ls-files | grep -E 'CMakeLists.txt|.*\.cmake$'); do
    pipx run cmake-format -i ./CMakeLists.txt
done
