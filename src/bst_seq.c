#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <pthread.h>
#include "bst.h"
#include <time.h>
typedef struct node {
    int key;
    struct node *left;
    struct node *right;
} node_t;

struct bst {
    node_t *root;
    pthread_mutex_t   lock;    
    pthread_rwlock_t  rwlock;  
};


static node_t *node_create(int key)
{
    node_t *n = (node_t *)malloc(sizeof(node_t));
    if (!n) return NULL;
    n->key  = key;
    n->left = NULL;
    n->right = NULL;
    return n;
}

static void node_destroy(node_t *n)
{
    if (!n) return;
    node_destroy(n->left);
    node_destroy(n->right);
    free(n);
}

static node_t *insert_rec(node_t *root, int key)
{
    if (!root) {
        return node_create(key);
    }
    if (key < root->key) {
        root->left = insert_rec(root->left, key);
    } else if (key > root->key) {
        root->right = insert_rec(root->right, key);
    } else {

    }
    return root;
}

static int search_rec(node_t *root, int key)
{
    if (!root) return 0;
    if (key == root->key) return 1;
    if (key < root->key)  return search_rec(root->left, key);
    else                  return search_rec(root->right, key);
}

static node_t *build_balanced_range(int lo, int hi)
{
    if (lo > hi) return NULL;

    int mid = lo + (hi - lo) / 2;
    node_t *root = node_create(mid);
    if (!root) return NULL;

    root->left  = build_balanced_range(lo, mid - 1);
    root->right = build_balanced_range(mid + 1, hi);
    return root;
}


bst_t *bst_create(void)
{
    bst_t *tree = (bst_t *)malloc(sizeof(bst_t));
    if (!tree) return NULL;
    tree->root = NULL;
    pthread_mutex_init(&tree->lock, NULL);
    pthread_rwlock_init(&tree->rwlock, NULL);
    return tree;
}

void bst_destroy(bst_t *tree)
{
    if (!tree) return;
    node_destroy(tree->root);
    pthread_mutex_destroy(&tree->lock);
    pthread_rwlock_destroy(&tree->rwlock);
    free(tree);
}

void bst_build_balanced(bst_t *tree, int n_keys)
{
    tree->root = build_balanced_range(0, n_keys - 1);
}

void bst_build_sequential(bst_t *tree, int n_keys)
{

    tree->root = NULL;
    for (int k = 0; k < n_keys; ++k) {
        tree->root = insert_rec(tree->root, k);
    }
}

void bst_build_random(bst_t *tree, int n_keys)
{
    tree->root = NULL;

    int *keys = (int *)malloc(sizeof(int) * n_keys);
    if (!keys) return;

    for (int i = 0; i < n_keys; ++i) {
        keys[i] = i;
    }

    srand((unsigned)time(NULL));
    for (int i = n_keys - 1; i > 0; --i) {
        int j = rand() % (i + 1);
        int tmp = keys[i];
        keys[i] = keys[j];
        keys[j] = tmp;
    }

    for (int i = 0; i < n_keys; ++i) {
        tree->root = insert_rec(tree->root, keys[i]);
    }

    free(keys);
}

void bst_insert_seq(bst_t *tree, int key)
{
    tree->root = insert_rec(tree->root, key);
}

int bst_search_seq(bst_t *tree, int key)
{
    return search_rec(tree->root, key);
}


void bst_insert_cg(bst_t *tree, int key)
{
    pthread_mutex_lock(&tree->lock);
    tree->root = insert_rec(tree->root, key);
    pthread_mutex_unlock(&tree->lock);
}

int bst_search_cg(bst_t *tree, int key)
{
    pthread_mutex_lock(&tree->lock);
    int res = search_rec(tree->root, key);
    pthread_mutex_unlock(&tree->lock);
    return res;
}


void bst_insert_rwlock(bst_t *tree, int key)
{
    pthread_rwlock_wrlock(&tree->rwlock);
    tree->root = insert_rec(tree->root, key);
    pthread_rwlock_unlock(&tree->rwlock);
}

int bst_search_rwlock(bst_t *tree, int key)
{
    int res;
    pthread_rwlock_rdlock(&tree->rwlock);
    res = search_rec(tree->root, key);
    pthread_rwlock_unlock(&tree->rwlock);
    return res;
}

