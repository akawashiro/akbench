#! /bin/bash

set -eux -o pipefail

PROJECT_ROOT=$(git rev-parse --show-toplevel)
cd "$PROJECT_ROOT"

for cc_file in $(git ls-files | grep -E '\.(cpp|h|hpp|c|cc|cxx)$'); do
    clang-format-18 -i "$cc_file"
done
for cmake_file in $(git ls-files | grep -E 'CMakeLists.txt|.*\.cmake$'); do
    pipx run cmake-format -i "$cmake_file"
done

# Format Python files with black and isort
if git ls-files | grep -q '\.py$'; then
    echo "Formatting Python files..."
    
    # Create temporary directory for virtual environment
    TEMP_VENV=$(mktemp -d)
    trap "rm -rf $TEMP_VENV" EXIT
    
    # Create virtual environment and install formatters
    python3 -m venv "$TEMP_VENV"
    source "$TEMP_VENV/bin/activate"
    pip install --quiet black isort
    
    # Format Python files
    for py_file in $(git ls-files | grep '\.py$'); do
        echo "Formatting $py_file"
        isort "$py_file"
        black "$py_file"
    done
    
    deactivate
fi
