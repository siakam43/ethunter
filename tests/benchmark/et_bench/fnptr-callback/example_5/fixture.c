/* ET-Bench fixture: fnptr-callback/example_5 */
/* Based on redis quicklistPopCustom pattern */
/* fnptr: saver, targets: _quicklistSaver, listPopSaver */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define QUICKLIST_HEAD 0
#define QUICKLIST_TAIL 1
#define LIST_HEAD 0
#define LIST_TAIL 1
#define unlikely(x) __builtin_expect(!!(x), 0)
#define OBJ_ENCODING_QUICKLIST 2
#define OBJ_ENCODING_LISTPACK  5

typedef struct quicklistNode {
    unsigned char *entry;
    size_t sz;
    int count;
} quicklistNode;

typedef struct quicklist {
    quicklistNode *head;
    quicklistNode *tail;
    long count;
    int fill;
} quicklist;

#define QL_NODE_IS_PLAIN(node) ((node)->count == 1)

typedef struct redisObject {
    int encoding;
    void *ptr;
} robj;

static void quicklistDelIndex(quicklist *ql, quicklistNode *node, void *prev) {
    ql->count--;
}

static robj *createStringObjectFromLongLong(long long value) {
    robj *o = malloc(sizeof(*o));
    o->encoding = OBJ_ENCODING_LISTPACK;
    o->ptr = malloc(32);
    snprintf(o->ptr, 32, "%lld", value);
    return o;
}

int quicklistPopCustom(quicklist *quicklist, int where, unsigned char **data,
                       size_t *sz, long long *sval,
                       void *(*saver)(unsigned char *data, size_t sz)) {
    quicklistNode *node = where == QUICKLIST_HEAD ? quicklist->head : quicklist->tail;
    if (!node) return 0;

    if (unlikely(QL_NODE_IS_PLAIN(node))) {
        if (data)
            *data = saver(node->entry, node->sz);
        if (sz)
            *sz = node->sz;
        quicklistDelIndex(quicklist, node, NULL);
        return 1;
    }
    return 0;
}

robj *listTypePop(robj *subject, int where) {
    robj *value = NULL;

    if (subject->encoding == OBJ_ENCODING_QUICKLIST) {
        long long vlong;
        int ql_where = where == LIST_HEAD ? QUICKLIST_HEAD : QUICKLIST_TAIL;
        if (quicklistPopCustom(subject->ptr, ql_where, (unsigned char **)&value,
                               NULL, &vlong, listPopSaver)) {
            if (!value)
                value = createStringObjectFromLongLong(vlong);
        }
    }
    return value;
}

int quicklistPop(quicklist *quicklist, int where, unsigned char **data,
                 size_t *sz, long long *slong) {
    unsigned char *vstr = NULL;
    size_t vlen = 0;
    long long vlong = 0;
    if (quicklist->count == 0)
        return 0;
    int ret = quicklistPopCustom(quicklist, where, &vstr, &vlen, &vlong,
                                 _quicklistSaver);
    if (data) *data = vstr;
    if (sz) *sz = vlen;
    if (slong) *slong = vlong;
    return ret;
}

void *_quicklistSaver(unsigned char *data, size_t sz) {
    unsigned char *copy = malloc(sz);
    if (copy) memcpy(copy, data, sz);
    return copy;
}

void *listPopSaver(unsigned char *data, size_t sz) {
    unsigned char *copy = malloc(sz + 1);
    if (copy) {
        memcpy(copy, data, sz);
        copy[sz] = '\0';
    }
    return copy;
}
