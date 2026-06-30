#include "test_helpers.h"
#include "workload.h"
#include <string.h>

static int test_ratios_exact(void) {
    // 80/15/5 of 10000 = 8000/1500/500
    wl_op_t ops[10000];
    int rc = wl_generate(ops, 10000, 80, 15, 5, 1024, 42);
    ASSERT_EQ_LONG(rc, 0, "wl_generate rc");

    long s = 0, i = 0, d = 0;
    for (int k = 0; k < 10000; ++k) {
        if (ops[k].kind == WL_OP_SEARCH) s++;
        else if (ops[k].kind == WL_OP_INSERT) i++;
        else if (ops[k].kind == WL_OP_DELETE) d++;
    }
    ASSERT_EQ_LONG(s, 8000, "search count");
    ASSERT_EQ_LONG(i, 1500, "insert count");
    ASSERT_EQ_LONG(d, 500,  "delete count");
    return 0;
}

static int test_seed_reproducible(void) {
    wl_op_t a[1000], b[1000];
    int rc1 = wl_generate(a, 1000, 80, 15, 5, 512, 12345);
    int rc2 = wl_generate(b, 1000, 80, 15, 5, 512, 12345);
    ASSERT_EQ_LONG(rc1, 0, "rc1");
    ASSERT_EQ_LONG(rc2, 0, "rc2");
    ASSERT(memcmp(a, b, sizeof(a)) == 0, "same seed produces same op stream");
    return 0;
}

static int test_seed_differs(void) {
    wl_op_t a[1000], b[1000];
    wl_generate(a, 1000, 80, 15, 5, 512, 1);
    wl_generate(b, 1000, 80, 15, 5, 512, 2);
    ASSERT(memcmp(a, b, sizeof(a)) != 0, "different seed produces different op stream");
    return 0;
}

static int test_keyspace_bounds(void) {
    wl_op_t ops[5000];
    wl_generate(ops, 5000, 100, 0, 0, 256, 7);
    for (int k = 0; k < 5000; ++k) {
        ASSERT(ops[k].key >= 0,    "key non-negative");
        ASSERT(ops[k].key < 256,   "key within keyspace");
    }
    return 0;
}

static int test_invalid_pct_returns_nonzero(void) {
    wl_op_t one;
    int rc = wl_generate(&one, 1, 50, 30, 30, 16, 1); // sums to 110
    ASSERT(rc != 0, "rc must be nonzero for bad percentages");
    return 0;
}

int main(void) {
    if (test_ratios_exact())             return 1;
    if (test_seed_reproducible())        return 1;
    if (test_seed_differs())             return 1;
    if (test_keyspace_bounds())          return 1;
    if (test_invalid_pct_returns_nonzero()) return 1;
    printf("OK test_workload\n");
    return 0;
}
