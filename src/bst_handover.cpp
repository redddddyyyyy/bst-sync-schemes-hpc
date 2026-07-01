#include "bst_handover.h"
#include <mutex>
#include <new>

namespace {

struct node_t {
    int key;
    node_t *left;
    node_t *right;
    std::mutex lock;
    explicit node_t(int k) : key(k), left(nullptr), right(nullptr) {}
};

static void destroy_subtree(node_t *n) {
    if (!n) return;
    destroy_subtree(n->left);
    destroy_subtree(n->right);
    delete n;
}

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
    destroy_subtree(t->root);
    delete t;
}

void bst_handover_insert(bst_handover_t *t, int key) {
    // Empty-tree case: protected by root_lock.
    std::unique_lock<std::mutex> root_guard(t->root_lock);
    if (!t->root) {
        t->root = new (std::nothrow) node_t(key);
        return;
    }
    node_t *cur = t->root;
    cur->lock.lock();
    root_guard.unlock();

    while (true) {
        if (key == cur->key) {
            cur->lock.unlock();
            return;  // duplicate: idempotent
        }
        node_t **slot = (key < cur->key) ? &cur->left : &cur->right;
        if (!*slot) {
            *slot = new (std::nothrow) node_t(key);
            cur->lock.unlock();
            return;
        }
        node_t *child = *slot;
        child->lock.lock();
        cur->lock.unlock();
        cur = child;
    }
}

int bst_handover_search(bst_handover_t *t, int key) {
    std::unique_lock<std::mutex> root_guard(t->root_lock);
    if (!t->root) return 0;
    node_t *cur = t->root;
    cur->lock.lock();
    root_guard.unlock();

    while (true) {
        if (key == cur->key) {
            cur->lock.unlock();
            return 1;
        }
        node_t **slot = (key < cur->key) ? &cur->left : &cur->right;
        if (!*slot) {
            cur->lock.unlock();
            return 0;
        }
        node_t *child = *slot;
        child->lock.lock();
        cur->lock.unlock();
        cur = child;
    }
}

}  // extern "C"
