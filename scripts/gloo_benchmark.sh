#! /bin/bash

set -eux -o pipefail

VENV_DIR="/tmp/ipc_bench_gloo_benchmark_venv"
if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
fi
source "$VENV_DIR/bin/activate"
pip install --upgrade pip
pip install torch

torchrun --nproc_per_node=2 --nnodes=1 gloo_benchmark.py ${@}
