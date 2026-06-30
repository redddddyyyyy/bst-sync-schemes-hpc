#ifndef WORKLOAD_H
#define WORKLOAD_H
#include <stdint.h>
#include <stddef.h>

typedef enum { WL_OP_SEARCH = 0, WL_OP_INSERT = 1, WL_OP_DELETE = 2 } wl_op_kind_t;

typedef struct {
    wl_op_kind_t kind;
    int          key;
} wl_op_t;

// Generate `n_ops` operations into the caller-allocated `out` array.
// `pct_search + pct_insert + pct_delete` must sum to 100.
// Keys are uniformly distributed in [0, keyspace).
// RNG is seeded by `seed` (xorshift64*, thread-deterministic — same seed -> same sequence).
// Returns 0 on success, non-zero on argument error.
int wl_generate(wl_op_t *out,
                size_t   n_ops,
                int      pct_search,
                int      pct_insert,
                int      pct_delete,
                int      keyspace,
                uint64_t seed);

#endif
