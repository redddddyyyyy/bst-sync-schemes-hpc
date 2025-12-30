# BST Synchronization Schemes (HPC Benchmark) — C / Pthreads

Benchmarking a Binary Search Tree (BST) search workload under multiple synchronization schemes to study scalability, contention, and throughput as thread count increases. Optional hardware counters are collected via **PAPI**.

## What’s in this repo

- `src/` — C source code (bench + modes + PAPI wrapper)
- `include/` — headers
- `scripts/` — reproducible sweep + summarize + plot scripts
- `data/results_raw.csv` — raw measurements (all reps)
- `results.csv` — summarized results (mean/std + IPC)
- `plots/` — generated plots (throughput + speedup)
- `docs/` — report / supporting docs (if present)

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

---

## Build

### Requirements
- `gcc` + `make`
- `pthread` (standard on Linux)
- **Optional:** PAPI (only needed for hardware counters)
- Python (only needed for plots)

### Compile
```bash
make clean
make USE_PAPI=1   # enable PAPI support (recommended if installed)
# or:
make USE_PAPI=0   # build without PAPI

# Run (single experiment)

# Usage:

./bench N_INIT N_SEARCH N_THREADS MODE [TREE]


Examples:

# Sequential baseline
PAPI=1 ./bench 1000000 50000000 1 seq bal

# Coarse-grained lock (8 threads)
PAPI=1 ./bench 1000000 50000000 8 cg bal

# Fine-grained RW-lock (8 threads)
PAPI=1 ./bench 1000000 50000000 8 fg bal

# Ideal upper bound (8 threads)
PAPI=1 ./bench 1000000 50000000 8 ideal bal


Notes:

Set PAPI=1 to enable counters at runtime (if compiled with USE_PAPI=1).

If PAPI is unavailable or blocked, the benchmark still runs and prints timing normally.

# Reproduce the full experiment 

This reproduces the full sweep, writes raw CSV, summarizes into results.csv, and generates plots into plots/.

1) Run sweep (raw data)
chmod +x scripts/run_sweep.sh
./scripts/run_sweep.sh


Output:

data/results_raw.csv

2) Summarize results (means/std/IPC)
python3 scripts/summarize_results.py


Output:

results.csv

3) Generate plots (venv recommended)
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python3 scripts/plot_results.py
deactivate


Output:

plots/throughput_vs_threads_bal.png

plots/speedup_vs_threads_bal.png
