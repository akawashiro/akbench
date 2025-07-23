#! /bin/bash

set -eux -o pipefail

for file in $(git ls-files | grep -E '\.(cpp|h|hpp|c|cc|cxx)$'); do
    clang-format-18 -i "$file"
done
pipx run cmake-format -i ./CMakeLists.txt
