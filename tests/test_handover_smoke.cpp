#include "test_helpers.h"
#include "bst_handover.h"

int main(void) {
    bst_handover_t *t = bst_handover_create();
    ASSERT(t != NULL, "bst_handover_create returned NULL");
    bst_handover_insert(t, 42);
    bst_handover_search(t, 42);
    bst_handover_destroy(t);
    printf("OK test_handover_smoke\n");
    return 0;
}
