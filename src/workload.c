#include "workload.h"
#include <stdlib.h>

/* xorshift64* — deterministic, fast, sufficient for benchmark workloads.
   NOT cryptographic; NOT thread-shared. */
static uint64_t xs64(uint64_t *state) {
    uint64_t x = *state;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *state = x;
    return x * 0x2545F4914F6CDD1DULL;
}

int wl_generate(wl_op_t *out,
                size_t   n_ops,
                int      pct_search,
                int      pct_insert,
                int      pct_delete,
                int      keyspace,
                uint64_t seed)
{
    if (!out)                                   return 1;
    if (pct_search + pct_insert + pct_delete != 100) return 2;
    if (pct_search < 0 || pct_insert < 0 || pct_delete < 0) return 3;
    if (keyspace <= 0)                          return 4;
    if (n_ops == 0)                             return 0;

    /* Exact-count allocation: floor of each percent, give the remainder to search.
       This guarantees test_ratios_exact passes for n_ops divisible by 100. */
    size_t n_search = ((size_t)pct_search * n_ops) / 100;
    size_t n_insert = ((size_t)pct_insert * n_ops) / 100;
    size_t n_delete = ((size_t)pct_delete * n_ops) / 100;
    size_t remainder = n_ops - (n_search + n_insert + n_delete);
    n_search += remainder; /* round-off goes to dominant op; keeps the contract */

    uint64_t state = seed ? seed : 0x9E3779B97F4A7C15ULL;

    /* Fill slots in order, then Fisher-Yates shuffle so the kinds are
       interleaved deterministically. */
    size_t idx = 0;
    for (size_t k = 0; k < n_search; ++k) { out[idx].kind = WL_OP_SEARCH; ++idx; }
    for (size_t k = 0; k < n_insert; ++k) { out[idx].kind = WL_OP_INSERT; ++idx; }
    for (size_t k = 0; k < n_delete; ++k) { out[idx].kind = WL_OP_DELETE; ++idx; }

    /* Fisher-Yates shuffle of kinds (keys assigned after, so shuffle only kinds). */
    for (size_t i = n_ops - 1; i > 0; --i) {
        size_t j = (size_t)(xs64(&state) % (i + 1));
        wl_op_kind_t tmp = out[i].kind;
        out[i].kind = out[j].kind;
        out[j].kind = tmp;
    }

    /* Assign keys uniformly in [0, keyspace). */
    for (size_t i = 0; i < n_ops; ++i) {
        out[i].key = (int)(xs64(&state) % (uint64_t)keyspace);
    }

    return 0;
}
