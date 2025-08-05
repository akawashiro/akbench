#!/usr/bin/env python3

import argparse
import logging
import os
import sys
import threading
import time
from datetime import datetime

import torch
import torch.distributed as dist


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
        description="Measure send/recv bandwidth using torch.distributed (Gloo backend)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""Log levels:
  DEBUG    - Detailed debug information
  INFO     - General information about benchmark progress  
  WARNING  - Warning messages (default)
  ERROR    - Error messages only
  CRITICAL - Critical errors only
        """.strip(),
    )

    parser.add_argument(
        "--data-size",
        type=int,
        default=1024**3,
        help="Size of data to send/recv in bytes. Default is 1GiByte.",
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


def run_gloo_benchmark(logger, data_size):
    """Run Gloo bandwidth benchmark and format results in GiB/s"""

    # Get rank and world size from environment
    local_rank = int(os.environ.get("LOCAL_RANK", 0))
    world_size = 2

    logger.info(f"Rank {local_rank}: Initializing process group.")
    dist.init_process_group("gloo", rank=local_rank, world_size=world_size)
    logger.info(f"Rank {local_rank}: Process group initialized successfully.")

    data_size_bytes = data_size
    # float32 is 4 bytes, so divide by 4 to get the number of elements
    tensor_size = data_size_bytes // 4
    tensor = torch.ones(tensor_size, dtype=torch.float32)

    warmup_iterations = 3
    measure_iterations = 10

    logger.info(f"Data size: {data_size_bytes / (1024**3):.2f} GiB")
    logger.info(f"Warmup iterations: {warmup_iterations}")
    logger.info(f"Repetitions: {measure_iterations}")
    logger.info("Starting benchmark...")

    if local_rank == 0:
        times = []

        for i in range(warmup_iterations + measure_iterations):
            start_time = time.time()
            logger.info(f"Rank {local_rank}: Sending tensor to rank 1.")
            dist.send(tensor, dst=1)
            logger.info(f"Rank {local_rank}: Receiving tensor from rank 1.")
            dist.recv(tensor, src=1)
            end_time = time.time()

            if i >= warmup_iterations:
                elapsed_time = end_time - start_time
                times.append(elapsed_time)
                logger.info(
                    f"Rank {local_rank}: Iteration {i-warmup_iterations+1} took {elapsed_time:.6f} seconds."
                )

        logger.info("Benchmark completed, calculating results...")

        average_time = sum(times) / len(times)
        # Multiply by 2 because we send and receive (round trip)
        bandwidth_bytes_per_sec = (data_size_bytes * 2) / average_time
        # Convert to GiB/s (1 GiB = 2^30 bytes)
        bandwidth_gib_s = bandwidth_bytes_per_sec / (1 << 30)

        logger.info(f"Raw result: {bandwidth_gib_s:.6f} GiB/s")

        # Final output to stdout in the same format as lmbench
        print(f"gloo_bandwidth: {bandwidth_gib_s:.2f} GiB/s")

        result = bandwidth_gib_s

    elif local_rank == 1:
        for i in range(warmup_iterations + measure_iterations):
            logger.info(f"Rank {local_rank}: Receiving tensor from rank 0.")
            dist.recv(tensor, src=0)
            logger.info(f"Rank {local_rank}: Sending tensor to rank 0.")
            dist.send(tensor, dst=0)

        result = None

    logger.info(f"Rank {local_rank}: Destroying process group.")
    dist.destroy_process_group()
    logger.info(f"Rank {local_rank}: Process group destroyed.")

    return result


def main():
    """Main function that parses arguments and runs the Gloo benchmark"""
    # Parse command line arguments
    parser = setup_argument_parser()
    args = parser.parse_args()

    # Setup logging with specified level
    logger = setup_logging(args.log_level)

    # Check if we're running in distributed mode
    if "LOCAL_RANK" not in os.environ:
        logger.error(
            "LOCAL_RANK environment variable not set. Please use torchrun to launch this script."
        )
        logger.error(
            "Example: torchrun --nproc_per_node=2 --nnodes=1 gloo_benchmark.py"
        )
        sys.exit(1)

    try:
        # Run the benchmark
        logger.info("Running Gloo bandwidth benchmark...")
        run_gloo_benchmark(logger, args.data_size)

    except Exception as e:
        logger.error(f"Error running Gloo benchmark: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
