#include "bst_handover.h"
#include <cstdlib>
#include <mutex>

namespace {

struct node_t {
    int key;
    node_t *left;
    node_t *right;
    std::mutex lock;
    explicit node_t(int k) : key(k), left(nullptr), right(nullptr) {}
};

}  // namespace

struct bst_handover {
    node_t *root;
    std::mutex root_lock;
    bst_handover() : root(nullptr) {}
};

extern "C" {

bst_handover_t *bst_handover_create(void) {
    return new (std::nothrow) bst_handover_t();
}

void bst_handover_destroy(bst_handover_t *t) {
    if (!t) return;
    delete t;
}

void bst_handover_insert(bst_handover_t *t, int key) {
    (void)t;
    (void)key;
}

int bst_handover_search(bst_handover_t *t, int key) {
    (void)t;
    (void)key;
    return 0;
}

}  // extern "C"
