import torch
import torch.distributed as dist
import argparse
import time
import logging
import os

_logger = logging.getLogger(__name__)
_logger.setLevel(logging.INFO)

console_handler = logging.StreamHandler()
formatter = logging.Formatter('%(levelname)s %(asctime)s %(filename)s:%(lineno)d] %(message)s', datefmt='%H:%M:%S')
console_handler.setFormatter(formatter)

_logger.addHandler(console_handler)


def run(rank, world_size, data_size):
    _logger.info(f"Rank {rank}: Initializing process group.")
    dist.init_process_group("gloo", rank=rank, world_size=world_size)
    _logger.info(f"Rank {rank}: Process group initialized successfully.")

    data_size_bytes = data_size
    # float32 is 4 bytes, so divide by 4 to get the number of elements
    tensor_size = data_size_bytes // 4
    tensor = torch.ones(tensor_size, dtype=torch.float32)

    warmup_iterations = 3
    measure_iterations = 10

    if rank == 0:
        _logger.info(f"Data size: {data_size_bytes} bytes ({data_size_bytes / (1024**3):.3f} GiB)")
        _logger.info("Starting warm-up phase...")

        times = []

        for i in range(warmup_iterations + measure_iterations):
            start_time = time.time()
            _logger.info(f"Rank {rank}: Sending tensor to rank 1.")
            dist.send(tensor, dst=1)
            _logger.info(f"Rank {rank}: Receiving tensor from rank 1.")
            dist.recv(tensor, src=1)
            end_time = time.time()

            if i >= warmup_iterations:
                elapsed_time = end_time - start_time
                times.append(elapsed_time)
                _logger.info(f"Rank {rank}: Iteration {i} took {elapsed_time:.6f} seconds.")

        average_time = sum(times) / len(times)
        bandwidth = (data_size_bytes * 2) / average_time
        _logger.info(f"Average bandwidth: {bandwidth / (1024**3):.2f} GiByte/sec")

    elif rank == 1:
        for i in range(warmup_iterations + measure_iterations):
            _logger.info(f"Rank {rank}: Receiving tensor from rank 0.")
            dist.recv(tensor, src=0)
            _logger.info(f"Rank {rank}: Sending tensor to rank 0.")
            dist.send(tensor, dst=0)
    
    _logger.info(f"Rank {rank}: Destroying process group.")
    dist.destroy_process_group()
    _logger.info(f"Rank {rank}: Process group destroyed.")

def main():
    """
    Parses arguments and provides instructions for running the script.
    """
    parser = argparse.ArgumentParser(description="Measure send/recv bandwidth using torch.distributed.")
    parser.add_argument("--data_size", type=int, default=1024**3,
                        help="Size of data to send/recv in bytes. Default is 1GiByte.")
    args = parser.parse_args()
    world_size = 2
    local_rank = int(os.environ["LOCAL_RANK"])
    run(rank=local_rank, world_size=world_size, data_size=args.data_size)

if __name__ == "__main__":
    main()
