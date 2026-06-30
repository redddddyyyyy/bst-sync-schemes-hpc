#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

# Backward-compat: old positional CLI still works
OUT1=$(./bin/bench 1000 10000 4 rwlock bal)
echo "$OUT1" | grep -q "Mode: rwlock" || { echo "FAIL: old CLI broken"; exit 1; }

# New CLI: --mix 100/0/0 with --ops 5000 → 5000 search ops, throughput line printed
OUT2=$(./bin/bench --mix 100/0/0 --ops 5000 --seed 42 1000 4 rwlock bal)
echo "$OUT2" | grep -q "Mode: rwlock"   || { echo "FAIL: new CLI mode line"; exit 1; }
echo "$OUT2" | grep -qE "ops_search=5000" || { echo "FAIL: search count not 5000"; exit 1; }
echo "$OUT2" | grep -qE "ops_insert=0"    || { echo "FAIL: insert count not 0"; exit 1; }
echo "$OUT2" | grep -qE "ops_delete=0"    || { echo "FAIL: delete count not 0"; exit 1; }

# New CLI: --mix 80/15/5 with --ops 10000 → 8000/1500/500
OUT3=$(./bin/bench --mix 80/15/5 --ops 10000 --seed 7 1000 4 rwlock bal)
echo "$OUT3" | grep -qE "ops_search=8000" || { echo "FAIL: mixed search count"; exit 1; }
echo "$OUT3" | grep -qE "ops_insert=1500" || { echo "FAIL: mixed insert count"; exit 1; }
echo "$OUT3" | grep -qE "ops_delete=500"  || { echo "FAIL: mixed delete count"; exit 1; }

echo "OK test_bench_cli"
