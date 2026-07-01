#include "test_helpers.h"
#include "bst_handover.h"

static int test_empty_search_returns_zero(void) {
    bst_handover_t *t = bst_handover_create();
    ASSERT_EQ_LONG(bst_handover_search(t, 42), 0, "search on empty tree");
    bst_handover_destroy(t);
    return 0;
}

static int test_insert_then_search_finds(void) {
    bst_handover_t *t = bst_handover_create();
    bst_handover_insert(t, 42);
    ASSERT_EQ_LONG(bst_handover_search(t, 42), 1, "found just-inserted key");
    ASSERT_EQ_LONG(bst_handover_search(t, 43), 0, "uninserted key not found");
    bst_handover_destroy(t);
    return 0;
}

static int test_many_inserts(void) {
    bst_handover_t *t = bst_handover_create();
    for (int i = 0; i < 1000; ++i) bst_handover_insert(t, i);
    long found = 0;
    for (int i = 0; i < 2000; ++i) {
        if (bst_handover_search(t, i)) found++;
    }
    ASSERT_EQ_LONG(found, 1000, "all 1000 inserted keys found, no extras");
    bst_handover_destroy(t);
    return 0;
}

static int test_duplicate_insert_is_idempotent(void) {
    bst_handover_t *t = bst_handover_create();
    bst_handover_insert(t, 5);
    bst_handover_insert(t, 5);
    bst_handover_insert(t, 5);
    ASSERT_EQ_LONG(bst_handover_search(t, 5), 1, "single key after triple insert");
    bst_handover_destroy(t);
    return 0;
}

int main(void) {
    if (test_empty_search_returns_zero())          return 1;
    if (test_insert_then_search_finds())           return 1;
    if (test_many_inserts())                       return 1;
    if (test_duplicate_insert_is_idempotent())     return 1;
    printf("OK test_handover_basic\n");
    return 0;
}
