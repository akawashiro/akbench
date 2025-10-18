#!/usr/bin/env python3
"""
Script to generate bandwidth and latency graphs from JSON benchmark results.

This script reads all JSON files in the results directory, extracts bandwidth
and latency data, and generates two graphs (bandwidth.png and latency.png).

Usage:
    python3 scripts/generate_graphs.py

The script can be run from any directory - it will automatically find the
repository root and work with the results directory.
"""

import json
from pathlib import Path

import matplotlib.colors as mcolors
import matplotlib.pyplot as plt
import numpy as np


def get_repo_root():
    """Get the repository root directory."""
    script_dir = Path(__file__).resolve().parent
    return script_dir.parent


def load_json_files(results_dir):
    """Load all JSON files from the results directory."""
    data = {}
    for json_file in results_dir.glob("*.json"):
        machine_name = json_file.stem
        with open(json_file, "r") as f:
            data[machine_name] = json.load(f)
    return data


def get_machine_color(machine_name, machine_names):
    """Get a consistent color for a machine name."""
    # Use a colormap to get distinct colors
    colors = list(mcolors.TABLEAU_COLORS.values())
    idx = sorted(machine_names).index(machine_name)
    return colors[idx % len(colors)]


def generate_bandwidth_graph(data, output_path):
    """Generate bandwidth comparison graph."""
    # Extract all unique benchmark names
    all_benchmarks = set()
    for machine_data in data.values():
        for bench in machine_data.get("bandwidth", []):
            all_benchmarks.add(bench["name"])

    benchmark_names = sorted(all_benchmarks)
    machine_names = sorted(data.keys())

    # Prepare data for plotting
    fig, ax = plt.subplots(figsize=(14, 8))

    x = np.arange(len(benchmark_names))
    width = 0.8 / len(machine_names)

    for i, machine_name in enumerate(machine_names):
        values = []
        errors = []

        machine_data = data[machine_name].get("bandwidth", [])
        bench_dict = {b["name"]: b for b in machine_data}

        for bench_name in benchmark_names:
            if bench_name in bench_dict:
                bench = bench_dict[bench_name]
                # Convert from Bytes/sec to GiByte/sec
                value_gib = bench["average"] / (1024**3)
                stddev_gib = bench["stddev"] / (1024**3)
                values.append(value_gib)
                errors.append(stddev_gib)
            else:
                values.append(0)
                errors.append(0)

        color = get_machine_color(machine_name, machine_names)
        ax.bar(
            x + i * width,
            values,
            width,
            label=machine_name,
            color=color,
            yerr=errors,
            capsize=3,
        )

    ax.set_xlabel("Benchmark", fontsize=12, fontweight="bold")
    ax.set_ylabel("Bandwidth (GiByte/sec)", fontsize=12, fontweight="bold")
    ax.set_title(
        "IPC Bandwidth Comparison (higher is better)", fontsize=14, fontweight="bold"
    )
    ax.set_xticks(x + width * (len(machine_names) - 1) / 2)
    ax.set_xticklabels(benchmark_names, rotation=45, ha="right")
    ax.legend()
    ax.grid(axis="y", alpha=0.3)

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Generated bandwidth graph: {output_path}")


def generate_latency_graph(data, output_path):
    """Generate latency comparison graph."""
    # Extract all unique benchmark names
    all_benchmarks = set()
    for machine_data in data.values():
        for bench in machine_data.get("latency", []):
            all_benchmarks.add(bench["name"])

    benchmark_names = sorted(all_benchmarks)
    machine_names = sorted(data.keys())

    # Prepare data for plotting
    fig, ax = plt.subplots(figsize=(12, 8))

    x = np.arange(len(benchmark_names))
    width = 0.8 / len(machine_names)

    for i, machine_name in enumerate(machine_names):
        values = []
        errors = []

        machine_data = data[machine_name].get("latency", [])
        bench_dict = {b["name"]: b for b in machine_data}

        for bench_name in benchmark_names:
            if bench_name in bench_dict:
                bench = bench_dict[bench_name]
                # Convert from seconds to nanoseconds
                value_ns = bench["average"] * 1e9
                stddev_ns = bench["stddev"] * 1e9
                values.append(value_ns)
                errors.append(stddev_ns)
            else:
                values.append(0)
                errors.append(0)

        color = get_machine_color(machine_name, machine_names)
        ax.bar(
            x + i * width,
            values,
            width,
            label=machine_name,
            color=color,
            yerr=errors,
            capsize=3,
        )

    ax.set_xlabel("Benchmark", fontsize=12, fontweight="bold")
    ax.set_ylabel("Latency (nanoseconds)", fontsize=12, fontweight="bold")
    ax.set_title(
        "IPC Latency Comparison (lower is better)", fontsize=14, fontweight="bold"
    )
    ax.set_xticks(x + width * (len(machine_names) - 1) / 2)
    ax.set_xticklabels(benchmark_names, rotation=45, ha="right")
    ax.legend()
    ax.grid(axis="y", alpha=0.3)

    # Use log scale if values span multiple orders of magnitude
    # Collect all non-zero values across all machines
    all_values = []
    for machine_name in machine_names:
        machine_data = data[machine_name].get("latency", [])
        bench_dict = {b["name"]: b for b in machine_data}
        for bench_name in benchmark_names:
            if bench_name in bench_dict:
                bench = bench_dict[bench_name]
                value_ns = bench["average"] * 1e9
                if value_ns > 0:
                    all_values.append(value_ns)

    if all_values and max(all_values) / min(all_values) > 100:
        ax.set_yscale("log")

    plt.tight_layout()
    plt.savefig(output_path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Generated latency graph: {output_path}")


def print_readme_instructions(readme_path):
    """Print instructions for adding graphs to README.md."""
    print(f"\nTo add the graphs to {readme_path}, add the following lines:")
    print("=" * 60)
    print("![Bandwidth Comparison](bandwidth.png)")
    print("![Latency Comparison](latency.png)")
    print("=" * 60)


def main():
    """Main function."""
    repo_root = get_repo_root()
    results_dir = repo_root / "results"

    # Load all JSON files
    print(f"Loading JSON files from {results_dir}")
    data = load_json_files(results_dir)

    if not data:
        print("No JSON files found in results directory")
        return

    print(f"Found {len(data)} machine(s): {', '.join(sorted(data.keys()))}")

    # Generate graphs
    bandwidth_output = results_dir / "bandwidth.png"
    latency_output = results_dir / "latency.png"

    generate_bandwidth_graph(data, bandwidth_output)
    generate_latency_graph(data, latency_output)

    # Print instructions for updating README
    readme_path = results_dir / "README.md"
    print_readme_instructions(readme_path)

    print("\nDone!")


if __name__ == "__main__":
    main()
