# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is **akbench** (formerly ipc-bench), an IPC and system performance benchmarking tool written in C++23. The project measures both bandwidth and latency for various inter-process communication mechanisms and system operations.

### Core Architecture

The project has a modular design with two main entry points:
- **akbench.cc**: Unified benchmark runner that combines both latency and bandwidth tests
- **bandwidth.cc** & **latency.cc**: Legacy separate executables (still available)

Each IPC/system operation is implemented as a separate library with a consistent interface:
- **Bandwidth libraries**: `*_bandwidth.{cc,h}` (memcpy, tcp, uds, pipe, fifo, mq, mmap, shm, etc.)
- **Latency libraries**: `*_latency.{cc,h}` (atomic, barrier, condition_variable, semaphore, syscall)

All libraries link against `AKBENCH_LIBS` which includes Abseil dependencies and common utilities.

## Development Commands

### Build
```bash
./scripts/build.sh
```
This script:
- Updates git submodules (Abseil)
- Uses clang++ with ccache for faster rebuilds
- Builds with RelWithDebInfo and Ninja
- Generates compile_commands.json for IDE support

### Format Code
```bash
./scripts/format.sh
```
Uses clang-format-18 for C++ files and cmake-format for CMake files.

### Run Tests
```bash
# Build first, then run tests
./scripts/build.sh
ctest --test-dir build
# or
cd build && ctest
```

Only one test currently exists: `barrier_test`

### Run Benchmarks
```bash
# Bandwidth benchmarks (1GB data, 10 iterations, 3 warmups)
./build/akbench/akbench --type=all_bandwidth --data_size=$((1 << 30)) --num_iterations=10 --num_warmups=3

# Latency benchmarks 
./build/akbench/akbench --type=all_latency

# Run everything
./build/akbench/akbench --type=all --data_size=$((1 << 30)) --num_iterations=10 --num_warmups=3

# Legacy individual binaries also available:
./build/akbench/bandwidth --type=all --data_size=$((1 << 30)) --num_iterations=10 --num_warmups=3
./build/akbench/latency --type=all
```

## Key Technical Details

- **C++23** standard required
- **Abseil** for logging, flags, synchronization primitives
- **CMake** build system with Ninja generator
- **ccache** for build acceleration
- Uses **Abseil flags** for command-line argument parsing
- Consistent measurement methodology with warmup iterations

## Coding Style Guidelines

### Functional Programming Style

This project follows functional programming principles where possible:

- **Pure functions**: Prefer functions without side effects that return consistent outputs for the same inputs
- **Const correctness**: Use `const` arguments and member functions extensively
- **Immutable data**: Prefer immutable data structures and avoid modifying parameters
- **Pass by const reference**: Use `const T&` for complex types to avoid unnecessary copies
- **Const member functions**: Mark member functions `const` when they don't modify object state

Example:
```cpp
// Good: pure function with const parameters
auto calculate_bandwidth(const std::vector<uint8_t>& data, 
                        const std::chrono::duration<double>& elapsed) const -> double {
    return static_cast<double>(data.size()) / elapsed.count();
}

// Avoid: non-const parameters and side effects
void process_data(std::vector<uint8_t>& data, bool& success);
```

### Header File Guidelines

- **Minimize includes**: Avoid including unnecessary files in header files to reduce compilation dependencies
- **Implementation details**: Keep implementation-specific includes (like `<stacktrace>`) in source files only

## Adding New Benchmarks

1. Create `new_benchmark.{cc,h}` in `akbench/`
2. Add library target in `akbench/CMakeLists.txt`
3. Link the library to the appropriate executable(s)
4. Follow existing patterns for flag handling and measurement loops
5. Use `common.h` utilities for data generation and verification

## Pre-PR Checklist

Before creating a pull request, always:

1. **Create a new branch** for your work:
```bash
git checkout -b your-feature-branch-name
```

2. **Build and format** to ensure code quality:
```bash
# Build the project to ensure compilation succeeds
./scripts/build.sh

# Format all code to maintain consistent style
./scripts/format.sh
```

These steps ensure code quality and consistency across the project.

## Git Commit Guidelines

- Do NOT add Claude as co-author in commit messages
- Keep commit messages concise and descriptive

## MPI Support

The project includes MPI bandwidth benchmarks (`mpi_bandwidth.cc`) which require MPI to be installed on the system.