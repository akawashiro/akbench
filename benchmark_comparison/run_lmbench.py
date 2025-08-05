#!/usr/bin/env python3

import argparse
import logging
import os
import subprocess
import sys
import threading
from datetime import datetime


class AklogFormatter(logging.Formatter):
    """Custom formatter to match aklog format"""

    def format(self, record):
        # Level to single char mapping
        level_map = {
            "DEBUG": "D",
            "INFO": "I",
            "WARNING": "W",
            "ERROR": "E",
            "CRITICAL": "F",  # FATAL
        }
        level_char = level_map.get(record.levelname, "U")

        # Format timestamp as MMDD HH:MM:SS.microseconds
        now = datetime.now()
        timestamp = now.strftime("%m%d %H:%M:%S.") + f"{now.microsecond:06d}"

        # Get process and thread IDs
        pid = os.getpid()
        thread_id = threading.current_thread().ident

        # Get just the filename
        filename = os.path.basename(record.pathname)

        # Format: {level}{timestamp} {pid} {thread_id} {filename}:{line}] {message}
        return f"{level_char}{timestamp} {pid}  {thread_id} {filename}:{record.lineno}] {record.getMessage()}"


def setup_argument_parser():
    """Setup command line argument parser"""
    parser = argparse.ArgumentParser(
        description="Run lmbench bandwidth benchmarks and output results in GiB/s",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Log levels:
  DEBUG    - Detailed debug information
  INFO     - General information about benchmark progress  
  WARNING  - Warning messages (default)
  ERROR    - Error messages only
  CRITICAL - Critical errors only
        """.strip(),
    )

    parser.add_argument(
        "--log-level",
        choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
        default="WARNING",
        help="Set the logging level (default: WARNING)",
    )

    return parser


def setup_logging(log_level):
    """Configure logging with custom formatter"""
    logger = logging.getLogger(__name__)

    # Clear any existing handlers
    logger.handlers.clear()

    # Set log level
    numeric_level = getattr(logging, log_level.upper())
    logger.setLevel(numeric_level)

    # Create handler with custom formatter
    handler = logging.StreamHandler(sys.stderr)
    handler.setFormatter(AklogFormatter())
    logger.addHandler(handler)

    return logger


def run_lmbench_bw_mem(logger):
    """Run lmbench bw_mem benchmark and format results in GiB/s"""

    # Path to the bw_mem binary
    script_dir = os.path.dirname(os.path.abspath(__file__))
    bw_mem_path = os.path.join(
        script_dir, "lmbench", "bin", "x86_64-linux-gnu", "bw_mem"
    )

    # Check if binary exists
    if not os.path.isfile(bw_mem_path):
        logger.error(f"bw_mem binary not found at {bw_mem_path}")
        logger.error("Please run setup_lmbench.sh first to build lmbench")
        sys.exit(1)

    # Command arguments
    warmup = 3
    repetitions = 10
    data_size = 1 << 30  # 1 GiB
    operation = "cp"

    # Build command
    cmd = [
        bw_mem_path,
        "-W",
        str(warmup),
        "-N",
        str(repetitions),
        str(data_size),
        operation,
    ]

    logger.info(f"Running: {' '.join(cmd)}")
    logger.info(f"Data size: {data_size / (1 << 30):.2f} GiB")
    logger.info(f"Warmup iterations: {warmup}")
    logger.info(f"Repetitions: {repetitions}")
    logger.info("Starting benchmark...")

    try:
        # Run the command
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)

        logger.info("Benchmark completed, parsing results...")

        # lmbench bw_mem outputs to stderr, not stdout
        output_lines = result.stderr.strip().split("\n")

        # The last line should contain the result in format: "size_mb bandwidth_mb/s"
        # Example: "1073.74 12345.67"
        if output_lines:
            last_line = output_lines[-1].strip()
            parts = last_line.split()

            if len(parts) >= 2:
                size_mb = float(parts[0])
                bandwidth_mb_s = float(parts[1])

                # Convert MB/s to GiB/s (1 MB = 10^6 bytes, 1 GiB = 2^30 bytes)
                bandwidth_gib_s = bandwidth_mb_s * 1e6 / (1 << 30)

                logger.info(f"Raw output: {last_line}")

                # Final output to stdout
                print(f"bw_mem cp: {bandwidth_gib_s:.2f} GiB/s")

                return bandwidth_gib_s
            else:
                logger.error(f"Could not parse output. Last line: {last_line}")
                logger.error(f"Full stderr: {repr(result.stderr)}")
                sys.exit(1)
        else:
            logger.error("No output from bw_mem")
            logger.error(f"Full stderr: {repr(result.stderr)}")
            sys.exit(1)

    except subprocess.CalledProcessError as e:
        logger.error(f"Error running bw_mem: {e}")
        if e.stderr:
            logger.error(f"stderr: {e.stderr}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        sys.exit(1)


def run_lmbench_bw_pipe(logger):
    """Run lmbench bw_pipe benchmark and format results in GiB/s"""

    # Path to the bw_pipe binary
    script_dir = os.path.dirname(os.path.abspath(__file__))
    bw_pipe_path = os.path.join(
        script_dir, "lmbench", "bin", "x86_64-linux-gnu", "bw_pipe"
    )

    # Check if binary exists
    if not os.path.isfile(bw_pipe_path):
        logger.error(f"bw_pipe binary not found at {bw_pipe_path}")
        logger.error("Please run setup_lmbench.sh first to build lmbench")
        sys.exit(1)

    # Command arguments
    warmup = 3
    repetitions = 10
    data_size = 1 << 30  # 1 GiB

    # Build command
    cmd = [
        bw_pipe_path,
        "-W",
        str(warmup),
        "-N",
        str(repetitions),
        "-M",
        str(data_size),
    ]

    logger.info(f"Running: {' '.join(cmd)}")
    logger.info(f"Data size: {data_size / (1 << 30):.2f} GiB")
    logger.info(f"Warmup iterations: {warmup}")
    logger.info(f"Repetitions: {repetitions}")
    logger.info("Starting benchmark...")

    try:
        # Run the command
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)

        logger.info("Benchmark completed, parsing results...")

        # Check both stdout and stderr for output
        output = result.stdout.strip()
        if not output:
            output = result.stderr.strip()

        # Parse output format: "Pipe bandwidth: 1234.56 MB/sec"
        if "Pipe bandwidth:" in output:
            parts = output.split()
            if len(parts) >= 4 and parts[0] == "Pipe" and parts[1] == "bandwidth:":
                bandwidth_mb_s = float(parts[2])

                # Convert MB/s to GiB/s (1 MB = 10^6 bytes, 1 GiB = 2^30 bytes)
                bandwidth_gib_s = bandwidth_mb_s * 1e6 / (1 << 30)

                logger.info(f"Raw output: {output}")

                # Final output to stdout
                print(f"bw_pipe: {bandwidth_gib_s:.2f} GiB/s")

                return bandwidth_gib_s
            else:
                logger.error(f"Could not parse output: {output}")
                sys.exit(1)
        else:
            logger.error(f"Unexpected output format: {output}")
            sys.exit(1)

    except subprocess.CalledProcessError as e:
        logger.error(f"Error running bw_pipe: {e}")
        if e.stderr:
            logger.error(f"stderr: {e.stderr}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        sys.exit(1)


def run_lmbench_bw_unix(logger):
    """Run lmbench bw_unix benchmark and format results in GiB/s"""

    # Path to the bw_unix binary
    script_dir = os.path.dirname(os.path.abspath(__file__))
    bw_unix_path = os.path.join(
        script_dir, "lmbench", "bin", "x86_64-linux-gnu", "bw_unix"
    )

    # Check if binary exists
    if not os.path.isfile(bw_unix_path):
        logger.error(f"bw_unix binary not found at {bw_unix_path}")
        logger.error("Please run setup_lmbench.sh first to build lmbench")
        sys.exit(1)

    # Command arguments
    warmup = 3
    repetitions = 10
    data_size = 1 << 30  # 1 GiB

    # Build command
    cmd = [
        bw_unix_path,
        "-W",
        str(warmup),
        "-N",
        str(repetitions),
        "-M",
        str(data_size),
    ]

    logger.info(f"Running: {' '.join(cmd)}")
    logger.info(f"Data size: {data_size / (1 << 30):.2f} GiB")
    logger.info(f"Warmup iterations: {warmup}")
    logger.info(f"Repetitions: {repetitions}")
    logger.info("Starting benchmark...")

    try:
        # Run the command
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)

        logger.info("Benchmark completed, parsing results...")

        # Check both stdout and stderr for output
        output = result.stdout.strip()
        if not output:
            output = result.stderr.strip()

        # Parse output format: "AF_UNIX sock stream bandwidth: 1234.56 MB/sec"
        if "AF_UNIX sock stream bandwidth:" in output:
            parts = output.split()
            if len(parts) >= 6 and parts[-1] == "MB/sec":
                bandwidth_mb_s = float(parts[-2])

                # Convert MB/s to GiB/s (1 MB = 10^6 bytes, 1 GiB = 2^30 bytes)
                bandwidth_gib_s = bandwidth_mb_s * 1e6 / (1 << 30)

                logger.info(f"Raw output: {output}")

                # Final output to stdout
                print(f"bw_unix: {bandwidth_gib_s:.2f} GiB/s")

                return bandwidth_gib_s
            else:
                logger.error(f"Could not parse output: {output}")
                sys.exit(1)
        else:
            logger.error(f"Unexpected output format: {output}")
            sys.exit(1)

    except subprocess.CalledProcessError as e:
        logger.error(f"Error running bw_unix: {e}")
        if e.stderr:
            logger.error(f"stderr: {e.stderr}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        sys.exit(1)


def run_lmbench_lat_syscall_single(logger, syscall_type, test_file=None):
    """Run a single lmbench lat_syscall benchmark and format results in nanoseconds"""

    # Path to the lat_syscall binary
    script_dir = os.path.dirname(os.path.abspath(__file__))
    lat_syscall_path = os.path.join(
        script_dir, "lmbench", "bin", "x86_64-linux-gnu", "lat_syscall"
    )

    # Check if binary exists
    if not os.path.isfile(lat_syscall_path):
        logger.error(f"lat_syscall binary not found at {lat_syscall_path}")
        logger.error("Please run setup_lmbench.sh first to build lmbench")
        sys.exit(1)

    # Command arguments
    warmup = 3
    repetitions = 10

    # Build command
    cmd = [
        lat_syscall_path,
        "-W",
        str(warmup),
        "-N",
        str(repetitions),
        syscall_type,
    ]

    # Add test file if required
    if test_file:
        cmd.append(test_file)

    logger.info(f"Running: {' '.join(cmd)}")
    logger.info(f"Syscall type: {syscall_type}")
    if test_file:
        logger.info(f"Test file: {test_file}")
    logger.info(f"Warmup iterations: {warmup}")
    logger.info(f"Repetitions: {repetitions}")
    logger.info("Starting benchmark...")

    try:
        # Run the command
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)

        logger.info("Benchmark completed, parsing results...")

        # Check both stdout and stderr for output
        output = result.stdout.strip()
        if not output:
            output = result.stderr.strip()

        # Parse different output formats
        syscall_patterns = {
            "null": ("Simple syscall:", "syscall"),
            "read": ("Simple read:", "read"),
            "write": ("Simple write:", "write"),
            "stat": ("Simple stat:", "stat"),
            "fstat": ("Simple fstat:", "fstat"),
            "open": ("Simple open/close:", "open/close"),
        }

        if syscall_type in syscall_patterns:
            pattern, display_name = syscall_patterns[syscall_type]
            if pattern in output and "microseconds" in output:
                parts = output.split()
                # Find the microseconds value (should be right before "microseconds")
                microseconds_idx = None
                for i, part in enumerate(parts):
                    if part == "microseconds":
                        microseconds_idx = i - 1
                        break

                if microseconds_idx is not None and microseconds_idx >= 0:
                    latency_microseconds = float(parts[microseconds_idx])

                    # Convert microseconds to nanoseconds
                    latency_nanoseconds = latency_microseconds * 1000

                    logger.info(f"Raw output: {output}")

                    # Final output to stdout
                    print(f"lat_syscall {syscall_type}: {latency_nanoseconds:.2f} ns")

                    return latency_nanoseconds
                else:
                    logger.error(
                        f"Could not find microseconds value in output: {output}"
                    )
                    sys.exit(1)
            else:
                logger.error(f"Unexpected output format for {syscall_type}: {output}")
                sys.exit(1)
        else:
            logger.error(f"Unknown syscall type: {syscall_type}")
            sys.exit(1)

    except subprocess.CalledProcessError as e:
        logger.error(f"Error running lat_syscall {syscall_type}: {e}")
        if e.stderr:
            logger.error(f"stderr: {e.stderr}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        sys.exit(1)


def run_lmbench_lat_syscall(logger):
    """Run all lmbench lat_syscall benchmarks and format results in nanoseconds"""

    # List of syscalls to test
    syscalls = [
        ("null", None),
        ("read", None),
        ("write", None),
        ("stat", "/tmp"),
        ("fstat", "/tmp"),
        ("open", "/tmp"),
    ]

    results = {}
    for syscall_type, test_file in syscalls:
        results[syscall_type] = run_lmbench_lat_syscall_single(
            logger, syscall_type, test_file
        )

    return results


def run_lmbench_lat_sem(logger):
    """Run lmbench lat_sem benchmark and format results in nanoseconds"""

    # Path to the lat_sem binary
    script_dir = os.path.dirname(os.path.abspath(__file__))
    lat_sem_path = os.path.join(
        script_dir, "lmbench", "bin", "x86_64-linux-gnu", "lat_sem"
    )

    # Check if binary exists
    if not os.path.isfile(lat_sem_path):
        logger.error(f"lat_sem binary not found at {lat_sem_path}")
        logger.error("Please run setup_lmbench.sh first to build lmbench")
        sys.exit(1)

    # Command arguments
    warmup = 3
    repetitions = 10

    # Build command
    cmd = [
        lat_sem_path,
        "-W",
        str(warmup),
        "-N",
        str(repetitions),
    ]

    logger.info(f"Running: {' '.join(cmd)}")
    logger.info(f"Warmup iterations: {warmup}")
    logger.info(f"Repetitions: {repetitions}")
    logger.info("Starting benchmark...")

    try:
        # Run the command
        result = subprocess.run(cmd, capture_output=True, text=True, check=True)

        logger.info("Benchmark completed, parsing results...")

        # Check both stdout and stderr for output
        output = result.stdout.strip()
        if not output:
            output = result.stderr.strip()

        # Parse output format: "Semaphore latency: 1.2366 microseconds"
        if "Semaphore latency:" in output and "microseconds" in output:
            parts = output.split()
            # Find the microseconds value (should be right before "microseconds")
            microseconds_idx = None
            for i, part in enumerate(parts):
                if part == "microseconds":
                    microseconds_idx = i - 1
                    break

            if microseconds_idx is not None and microseconds_idx >= 0:
                latency_microseconds = float(parts[microseconds_idx])

                # Convert microseconds to nanoseconds
                latency_nanoseconds = latency_microseconds * 1000

                logger.info(f"Raw output: {output}")

                # Final output to stdout
                print(f"lat_sem: {latency_nanoseconds:.2f} ns")

                return latency_nanoseconds
            else:
                logger.error(f"Could not find microseconds value in output: {output}")
                sys.exit(1)
        else:
            logger.error(f"Unexpected output format: {output}")
            sys.exit(1)

    except subprocess.CalledProcessError as e:
        logger.error(f"Error running lat_sem: {e}")
        if e.stderr:
            logger.error(f"stderr: {e.stderr}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    # Parse command line arguments
    parser = setup_argument_parser()
    args = parser.parse_args()

    # Setup logging with specified level
    logger = setup_logging(args.log_level)

    # Run all benchmarks
    logger.info("Running lmbench benchmarks...")

    # Run bandwidth benchmarks
    run_lmbench_bw_mem(logger)
    run_lmbench_bw_pipe(logger)
    run_lmbench_bw_unix(logger)

    # Run latency benchmarks
    run_lmbench_lat_syscall(logger)
    run_lmbench_lat_sem(logger)
