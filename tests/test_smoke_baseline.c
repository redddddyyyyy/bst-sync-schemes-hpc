#include "test_helpers.h"
#include "bst.h"

int main(void) {
    bst_t *t = bst_create();
    ASSERT(t != NULL, "bst_create returned NULL");

    bst_build_balanced(t, 1000);

    long found = 0;
    for (int i = 0; i < 2000; ++i) {
        if (bst_search_seq(t, i)) found++;
    }
    ASSERT_EQ_LONG(found, 1000, "balanced tree search hit count");

    bst_destroy(t);
    printf("OK test_smoke_baseline\n");
    return 0;
}
