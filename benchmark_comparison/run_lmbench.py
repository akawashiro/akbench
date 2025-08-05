#!/usr/bin/env python3

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


# Configure logging with custom formatter
logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
handler = logging.StreamHandler(sys.stderr)
handler.setFormatter(AklogFormatter())
logger.addHandler(handler)


def run_lmbench_bw_mem():
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
                print(f"{bandwidth_gib_s:.2f} GiB/s")

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


if __name__ == "__main__":
    run_lmbench_bw_mem()
