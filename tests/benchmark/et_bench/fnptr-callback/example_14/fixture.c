/* ET-Bench fixture: fnptr-callback/example_14 */
/* Based on GCC's gt_pch_save / gt_pch_p_14lang_tree_node pattern */
/* fnptr: op, targets: relocate_ptrs */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define ATTRIBUTE_UNUSED

typedef struct tree_node {
    int code;
    struct tree_node *next_variant;
    struct tree_node *chain;
    struct tree_node *type;
} *tree;

typedef union lang_tree_node {
    struct tree_node generic;
} union_lang_tree_node;

int lto_tree_node_structure(union_lang_tree_node *x) { return 0; }
int tree_node_structure(struct tree_node *x) { return 0; }

#define TS_LTO_GENERIC 0
#define TS_BASE 0
#define TS_TYPED 1

typedef struct ptr_data {
    void *obj;
    size_t size;
    void (*note_ptr_fn)(void *, void *, void *);
    void *note_ptr_cookie;
    void *new_addr;
} ptr_data_t;

typedef struct {
    ptr_data_t **slots;
    size_t capacity;
    size_t count;
} htab_t;

htab_t *saving_htab;

#define POINTER_HASH(p) ((uintptr_t)(p) >> 3)
#define INSERT 1

static ptr_data_t **htab_find_slot_with_hash(htab_t *ht, void *obj, uintptr_t hash, int insert) {
    for (size_t i = 0; i < ht->count; i++) {
        if (ht->slots[i] && ht->slots[i]->obj == obj)
            return &ht->slots[i];
    }
    if (insert && ht->count < ht->capacity) {
        ht->slots[ht->count] = calloc(1, sizeof(ptr_data_t));
        return &ht->slots[ht->count++];
    }
    return NULL;
}

static ptr_data_t *htab_find_with_hash(htab_t *ht, void *obj, uintptr_t hash) {
    for (size_t i = 0; i < ht->count; i++) {
        if (ht->slots[i] && ht->slots[i]->obj == obj)
            return ht->slots[i];
    }
    return NULL;
}

typedef struct traversal_state {
    ptr_data_t **ptrs;
    size_t ptrs_i;
    size_t num_ptrs;
} traversal_state_t;

void
relocate_ptrs (void *ptr_p, void *real_ptr_p, void *state_p)
{
    void **ptr = (void **)ptr_p;
    traversal_state_t *state
        = (traversal_state_t *)state_p;
    ptr_data_t *result;

    if (*ptr == NULL || *ptr == (void *)1)
        return;

    result = htab_find_with_hash(saving_htab, *ptr, POINTER_HASH(*ptr));
    if (!result) return;
    *ptr = result->new_addr;
    if (ptr_p == real_ptr_p)
        return;
}

void
gt_pch_p_14lang_tree_node (void *this_obj,
    void *x_p,
    void (*op)(void *, void *, void *),
    void *cookie)
{
    union_lang_tree_node * x = (union_lang_tree_node *)x_p;
    switch (lto_tree_node_structure(x))
    {
    case TS_LTO_GENERIC:
        switch (tree_node_structure(&(x->generic)))
        {
        case TS_BASE:
            break;
        case TS_TYPED:
            if ((void *)(x) == this_obj)
                op(&(x->generic.type), NULL, cookie);
            break;
        }
    }
}

int
gt_pch_note_object (void *obj, void *note_ptr_cookie,
            void (*note_ptr_fn)(void *, void *, void *),
            size_t length_override)
{
    ptr_data_t **slot;

    if (obj == NULL || obj == (void *) 1)
        return 0;

    slot = htab_find_slot_with_hash(saving_htab, obj, POINTER_HASH(obj), INSERT);
    if (slot && *slot != NULL)
    {
        return 0;
    }

    if (slot) {
        *slot = calloc(1, sizeof(ptr_data_t));
        (*slot)->obj = obj;
        (*slot)->note_ptr_fn = note_ptr_fn;
        (*slot)->note_ptr_cookie = note_ptr_cookie;
        if (length_override != (size_t)-1)
            (*slot)->size = length_override;
        else
            (*slot)->size = sizeof(void *);
    }
    return 1;
}

void
gt_pch_nx_lang_tree_node (void *x_p)
{
    union_lang_tree_node * x = (union_lang_tree_node *)x_p;
    union_lang_tree_node * xlimit = x;
    while (gt_pch_note_object(xlimit, xlimit,
           gt_pch_p_14lang_tree_node, (size_t)-1))
        xlimit = (union_lang_tree_node *)(xlimit->generic.chain);
}

void
gt_pch_save (FILE *f)
{
    traversal_state_t state;
    memset(&state, 0, sizeof(state));
    state.ptrs = saving_htab->slots;
    state.num_ptrs = saving_htab->count;

    for (state.ptrs_i = 0; state.ptrs_i < state.num_ptrs; state.ptrs_i++) {
        if (state.ptrs[state.ptrs_i] && state.ptrs[state.ptrs_i]->note_ptr_fn) {
            state.ptrs[state.ptrs_i]->note_ptr_fn(
                state.ptrs[state.ptrs_i]->obj,
                state.ptrs[state.ptrs_i]->note_ptr_cookie,
                relocate_ptrs);
        }
    }
}
