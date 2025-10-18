#! /bin/bash
set -eux -o pipefail
SCRIPT_DIR="$(realpath $(dirname "$0"))"
VENV_DIR=$(mktemp -d)
python3 -m venv "${VENV_DIR}"
source "${VENV_DIR}/bin/activate"
pip install --upgrade pip
pip install matplotlib
${SCRIPT_DIR}/generate_graphs.py "$@"
