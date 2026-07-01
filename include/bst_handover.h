#ifndef BST_HANDOVER_H
#define BST_HANDOVER_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bst_handover bst_handover_t;

bst_handover_t *bst_handover_create(void);
void            bst_handover_destroy(bst_handover_t *t);
void            bst_handover_insert(bst_handover_t *t, int key);
int             bst_handover_search(bst_handover_t *t, int key);

#ifdef __cplusplus
}
#endif

#endif  /* BST_HANDOVER_H */
