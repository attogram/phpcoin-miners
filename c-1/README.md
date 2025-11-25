# PHPCoin C Miner (`minerv2`)

This project provides a high-performance, multi-threaded CPU miner for PHPCoin, rewritten in C for significantly improved performance over the original PHP implementation.

## System Architecture

The project is composed of two main parts:
1.  **Core C Library (`src/miner_core.c`)**: Contains the C implementations of the four performance-critical mining functions: `calculate_argon_hash`, `calculate_nonce`, `calculate_hit`, and `calculate_target`.
2.  **Multi-threaded C Miner (`src/c_miner.c`)**: A complete, standalone miner that uses the core library and `pthreads` to perform mining operations in parallel. It is statically linked for maximum portability.

## Build Environment Setup

To compile the C miner, you will need `gcc`, `make`, and the development headers for the required libraries. On a standard Ubuntu system, these can be installed with the following command:

```bash
sudo apt-get update
sudo apt-get install -y build-essential libgmp-dev libssl-dev libargon2-dev libcurl4-openssl-dev
```

## Compilation and Execution

A `Makefile` is provided for easy compilation. All compiled binaries are statically linked and aggressively optimized using `-O3`, `-march=native`, and `-funroll-loops` for maximum performance.

To compile the miner:
```bash
cd utils/minerv2
make all
```

### Running the C Miner

The compiled `c_miner` executable is a standalone, multi-threaded miner. To run it, you need to provide the node URL, your PHPCoin address, and the desired number of threads.

```bash
cd utils/minerv2
./c_miner -n <node_url> -a <your_address> -t <num_threads>

# Example:
./c_miner -n https://main1.phpcoin.net -a PZ8Tyr4Nx8... -t 8
```
