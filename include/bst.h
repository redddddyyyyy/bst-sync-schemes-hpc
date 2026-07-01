#ifndef BST_H
#define BST_H

#include <stdint.h>

typedef struct bst bst_t;

// Create / destroy tree
bst_t *bst_create(void);
void   bst_destroy(bst_t *tree);

// Build trees
void bst_build_balanced(bst_t *tree, int n_keys);  // keys [0, n_keys-1], perfect BST
void bst_build_sequential(bst_t *tree, int n_keys); // insert 0..n-1 in order (unbalanced)
void bst_build_random(bst_t *tree, int n_keys, uint64_t seed);  // insert 0..n-1 in random order; seed drives the shuffle

// Sequential (no-lock) operations
void bst_insert_seq(bst_t *tree, int key);
int  bst_search_seq(bst_t *tree, int key);

// Coarse-grained global lock operations
void bst_insert_cg(bst_t *tree, int key);
int  bst_search_cg(bst_t *tree, int key);

// Global RW-lock operations (NOT per-node; see docs/analysis.md)
void bst_insert_rwlock(bst_t *tree, int key);
int  bst_search_rwlock(bst_t *tree, int key);

#endif

