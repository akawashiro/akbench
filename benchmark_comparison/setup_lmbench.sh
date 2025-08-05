#!/bin/bash

set -eux pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LMBENCH_DIR="$SCRIPT_DIR/lmbench"
LMBENCH_COMMIT="47abeb1798521d935424d6422b7b0da2b7b74e5a"

rm -rf "$LMBENCH_DIR"
git clone https://github.com/intel/lmbench.git "$LMBENCH_DIR"
cd "$LMBENCH_DIR"
git checkout "$LMBENCH_COMMIT"
make build CPPFLAGS="-I /usr/include/tirpc" LDFLAGS="-ltirpc" -j$(nproc)
