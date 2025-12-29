# BST Synchronization Schemes (HPC Benchmark) — C / Pthreads

Benchmarking a Binary Search Tree (BST) search workload under multiple synchronization schemes to study scalability, contention, and throughput vs. thread count.

## Modes
Run everything through the `bench` driver:

- `seq`   — sequential search (no locks)
- `cg`    — coarse-grained global mutex
- `fg`    — global RW-lock (concurrent readers)
- `ideal` — parallel read-only search, no locks (upper bound)

Tree construction options:
- `bal`  — perfectly balanced (default)
- `seq`  — sequential inserts `0..N_INIT-1` (unbalanced)
- `rand` — random insert order

## Build
Requires: `gcc`, `make`, `pthread`, and `papi` (optional counters).

```bash
make clean
make

