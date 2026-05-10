/* ET-Bench fixture: fnptr-callback/example_7 */
/* Based on redis defragStreamConsumer pattern */
/* fnptr: element_cb, targets: defragStreamConsumerPendingEntry, defragStreamConsumer, defragStreamConsumerGroup */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x) ((void)(x))

typedef struct sds {
    char *buf;
    size_t len;
} sds;

typedef struct streamConsumer {
    char *name;
    void *pel;
} streamConsumer;

typedef struct streamCG {
    void *consumers;
    void *pel;
} streamCG;

typedef struct rax {
    int size;
    void *data;
} rax;

typedef struct raxNode {
    int dummy;
} raxNode;

typedef struct raxIterator {
    raxNode *node;
    void *data;
    void *key;
    size_t key_len;
} raxIterator;

typedef struct dictEntry {
    void *val;
} dictEntry;

typedef struct redisDb {
    int id;
} redisDb;

typedef struct robj {
    int type;
    int encoding;
    void *ptr;
} robj;

typedef struct stream {
    rax *rax;
    rax *cgroups;
} stream;

#define OBJ_STREAM 5
#define OBJ_ENCODING_STREAM 3

typedef void* (*raxDefragFunction)(raxIterator *ri, void *privdata);

int raxNext(raxIterator *ri);
void raxStop(raxIterator *ri);
void raxSetData(raxNode *node, void *data);
int raxSize(rax *r);
void *activeDefragAlloc(void *p);
char *activeDefragSds(char *s);
void defragLater(redisDb *db, dictEntry *kde);
struct { int active_defrag_max_scan_fields; } server = { 256 };

void defragRadixTree(rax **raxref, int defrag_data, raxDefragFunction *element_cb, void *element_cb_data) {
    raxIterator ri;
    rax *r;

    memset(&ri, 0, sizeof(ri));
    while (raxNext(&ri)) {
        void *newdata = NULL;
        if (element_cb)
            newdata = element_cb(&ri, element_cb_data);
        if (defrag_data && !newdata)
            newdata = activeDefragAlloc(ri.data);
        if (newdata)
            raxSetData(ri.node, ri.data = newdata);
    }
    raxStop(&ri);
}

void* defragStreamConsumerPendingEntry(raxIterator *ri, void *privdata) {
    /* defrag a pending entry in the consumer's PEL */
    UNUSED(privdata);
    void *entry = ri->data;
    void *newentry = activeDefragAlloc(entry);
    return newentry;
}

void* defragStreamConsumer(raxIterator *ri, void *privdata) {
    streamConsumer *c = ri->data;
    streamCG *cg = privdata;
    void *newc = activeDefragAlloc(c);
    if (newc) {
        c = newc;
    }
    char *newsds = activeDefragSds(c->name);
    if (newsds)
        c->name = newsds;
    if (c->pel) {
        defragRadixTree((rax **)&c->pel, 0, defragStreamConsumerPendingEntry, (void *)c);
    }
    return newc;
}

void* defragStreamConsumerGroup(raxIterator *ri, void *privdata) {
    streamCG *cg = ri->data;
    UNUSED(privdata);
    if (cg->consumers)
        defragRadixTree((rax **)&cg->consumers, 0, defragStreamConsumer, cg);
    if (cg->pel)
        defragRadixTree((rax **)&cg->pel, 0, NULL, NULL);
    return NULL;
}

void defragStream(redisDb *db, dictEntry *kde) {
    robj *ob = (robj *)kde->val;
    stream *s = (stream *)ob->ptr, *news;

    /* handle the main struct */
    if ((news = (stream *)activeDefragAlloc(s)))
        ob->ptr = s = news;

    if (raxSize(s->rax) > server.active_defrag_max_scan_fields) {
        rax *newrax = (rax *)activeDefragAlloc(s->rax);
        if (newrax)
            s->rax = newrax;
        defragLater(db, kde);
    } else
        defragRadixTree(&s->rax, 1, NULL, NULL);

    if (s->cgroups)
        defragRadixTree((rax **)&s->cgroups, 1, defragStreamConsumerGroup, NULL);
}

int raxNext(raxIterator *ri) { return 0; }
void raxStop(raxIterator *ri) {}
void raxSetData(raxNode *node, void *data) {}
int raxSize(rax *r) { return r ? r->size : 0; }
void *activeDefragAlloc(void *p) { return p; }
char *activeDefragSds(char *s) { return s; }
void defragLater(redisDb *db, dictEntry *kde) {}
