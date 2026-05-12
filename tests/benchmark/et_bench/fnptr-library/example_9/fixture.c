/* ET-Bench fixture: fnptr-library/example_9 */
/* fnptr: list->dtor, targets: fileinfo_dtor, hash_element_dtor, free_bundle_hash_entry, freednsentry, trhash_dtor, sh_freeentry, curl_free, gsasl_free */
/* Pattern: library linked list with dtor function pointer, called on element removal */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZERO_NULL 0
#define CURL_MAX_INPUT_LENGTH 1048576
#define DEBUGASSERT(x)

typedef void (*Curl_hash_dtor)(void *, void *);

typedef struct Curl_llist_element {
    struct Curl_llist_element *prev;
    struct Curl_llist_element *next;
    void *ptr;
} Curl_llist_element;

typedef struct Curl_llist {
    Curl_llist_element *head;
    Curl_llist_element *tail;
    size_t size;
    void (*dtor)(void *, void *);
} Curl_llist;

void
Curl_llist_remove(struct Curl_llist *list, struct Curl_llist_element *e,
                  void *user)
{
    void *ptr;
    if (!e || list->size == 0)
        return;

    ptr = e->ptr;
    --list->size;

    /* call the dtor() last for when it actually frees the 'e' memory itself */
    if (list->dtor)
        list->dtor(user, ptr);
}

typedef struct bufref {
    const unsigned char *ptr;
    size_t len;
    void (*dtor)(void *);
} bufref_t;

void Curl_bufref_set(bufref_t *br, const void *ptr, size_t len,
                     void (*dtor)(void *))
{
    DEBUGASSERT(ptr || !len);
    DEBUGASSERT(len <= CURL_MAX_INPUT_LENGTH);
    br->ptr = (const unsigned char *)ptr;
    br->len = len;
    br->dtor = dtor;
}

typedef int (*hash_function)(void *, size_t);
typedef int (*comp_function)(void *, size_t, void *, size_t);

typedef struct Curl_hash {
    Curl_llist *table;
    int slots;
    hash_function hash_func;
    comp_function comp_func;
    Curl_hash_dtor dtor;
    size_t size;
} Curl_hash;

void Curl_hash_init(Curl_hash *h, int slots, hash_function hfunc,
                    comp_function comparator, Curl_hash_dtor dtor)
{
    DEBUGASSERT(h);
    DEBUGASSERT(slots);
    DEBUGASSERT(hfunc);
    DEBUGASSERT(comparator);
    DEBUGASSERT(dtor);
    h->table = NULL;
    h->hash_func = hfunc;
    h->comp_func = comparator;
    h->dtor = dtor;
    h->size = 0;
    h->slots = slots;
}

void Curl_llist_init(Curl_llist *l, void (*dtor)(void *, void *))
{
    l->size = 0;
    l->dtor = dtor;
    l->head = NULL;
    l->tail = NULL;
}

/* Targets: various dtor functions passed through init chains */
void fileinfo_dtor(void *user, void *ptr) { (void)user; free(ptr); }
void hash_element_dtor(void *user, void *ptr) { (void)user; free(ptr); }
void free_bundle_hash_entry(void *user, void *ptr) { (void)user; free(ptr); }
void freednsentry(void *user, void *ptr) { (void)user; free(ptr); }
void trhash_dtor(void *user, void *ptr) { (void)user; free(ptr); }
void sh_freeentry(void *user, void *ptr) { (void)user; free(ptr); }
void curl_free(void *p) { free(p); }
void gsasl_free(void *p) { free(p); }

/* Registration: bind all dtor targets */
void register_all_dtors(void) {
    Curl_llist l1, l2, l3, l4, l5, l6;
    Curl_llist_init(&l1, fileinfo_dtor);
    Curl_llist_init(&l2, hash_element_dtor);
    Curl_llist_init(&l3, free_bundle_hash_entry);
    Curl_llist_init(&l4, freednsentry);
    Curl_llist_init(&l5, trhash_dtor);
    Curl_llist_init(&l6, sh_freeentry);
    Curl_hash h1, h2;
    Curl_hash_init(&h1, 16, ((void *)0), ((void *)0), (Curl_hash_dtor)curl_free);
    Curl_hash_init(&h2, 16, ((void *)0), ((void *)0), (Curl_hash_dtor)gsasl_free);
    (void)h1; (void)h2;
}
