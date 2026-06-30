#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

N_INIT=1000000
N_SEARCH=50000000
TREE=bal
REPS=3
THREADS_LIST=(1 2 4 8 16 32 64)

mkdir -p data
OUTFILE="data/results_raw.csv"

# Allow toggling PAPI: PAPI_ON=0 ./scripts/run_sweep.sh
PAPI_ON="${PAPI_ON:-1}"

echo "mode,tree,n_init,n_search,threads,rep,elapsed_s,throughput_mops,instructions_total,cycles_total" > "$OUTFILE"

run_one () {
  local MODE="$1" TREE="$2" N_INIT="$3" N_SEARCH="$4" TH="$5" REP="$6"

  # Run bench (expects bin/bench after restructure)
  local OUT
  OUT=$(PAPI="$PAPI_ON" bin/bench "$N_INIT" "$N_SEARCH" "$TH" "$MODE" "$TREE")

  # Parse fields
  local INS CYC ELAP THR
  INS=$(echo "$OUT" | awk -F': ' '/INSTRUCTION_RETIRED \(TOTAL\)/{print $2}' | tr -d '\r')
  CYC=$(echo "$OUT" | awk -F': ' '/UNHALTED_CORE_CYCLES \(TOTAL\)/{print $2}' | tr -d '\r')
  ELAP=$(echo "$OUT" | awk '/Elapsed time:/{print $(NF-1)}' | tr -d '\r')

  # Throughput computed from elapsed
  THR=$(python3 - <<PY
n_search=$N_SEARCH
elapsed=float("$ELAP")
print((n_search/elapsed)/1e6)
PY
)

  INS=${INS:-0}
  CYC=${CYC:-0}

  echo "$MODE,$TREE,$N_INIT,$N_SEARCH,$TH,$REP,$ELAP,$THR,$INS,$CYC" >> "$OUTFILE"
}

echo "Running sweep: TREE=$TREE N_INIT=$N_INIT N_SEARCH=$N_SEARCH REPS=$REPS (PAPI_ON=$PAPI_ON)"
echo "Output -> $OUTFILE"

# Baseline once (seq is single-thread)
for rep in $(seq 1 "$REPS"); do
  run_one seq "$TREE" "$N_INIT" "$N_SEARCH" 1 "$rep"
done

for th in "${THREADS_LIST[@]}"; do
  for rep in $(seq 1 "$REPS"); do
    run_one cg    "$TREE" "$N_INIT" "$N_SEARCH" "$th" "$rep"
    run_one rwlock "$TREE" "$N_INIT" "$N_SEARCH" "$th" "$rep"
    run_one ideal "$TREE" "$N_INIT" "$N_SEARCH" "$th" "$rep"
  done
done

echo "Done."

