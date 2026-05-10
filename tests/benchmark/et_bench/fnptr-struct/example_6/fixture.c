/* ET-Bench fixture: fnptr-struct/example_6 */
/* Scenario: Redis dict defrag — defragAlloc from dictDefragFunctions.
   fnptr: defragfns->defragAlloc (extracted as local defragalloc)
   target: activeDefragAlloc
   caller: dictDefragBucket */

#include <stddef.h>
#include <stdlib.h>

#define entryIsKey(de) (((unsigned long)(de) & 0x1) == 0)
#define entryIsNoValue(de) (((unsigned long)(de) & 0x2) == 0)
#define ENTRY_PTR_NO_VALUE 0x2
#define dictGetKey(de) ((void *)(de))
#define dictGetVal(de) ((void *)(de))
#define encodeMaskPtr(p, m) ((void *)((unsigned long)(p) | (m)))
#define decodeEntryNoValue(de) ((dictEntryNoValue *)((unsigned long)(de) & ~0x2UL))

typedef struct dictEntry dictEntry;
typedef struct dict dict;

typedef void *(*dictDefragAllocFunction)(void *ptr);

typedef struct {
    dictDefragAllocFunction *defragAlloc;
    dictDefragAllocFunction *defragKey;
    dictDefragAllocFunction *defragVal;
} dictDefragFunctions;

typedef void (*dictScanFunction)(dict *d, dictEntry *entry, void *privdata);

typedef struct dictEntryNoValue {
    void *key;
} dictEntryNoValue;

typedef struct {
    unsigned long size;
    int rehashidx;
    unsigned long **ht_table;
} dictHash;

struct dict {
    dictHash ht_table[2];
    int iterators;
};

static void *activeDefragAlloc(void *ptr)
{
    if (ptr) return ptr;
    return NULL;
}

static void *activeDefragSds(void *ptr)
{
    return ptr;
}

/* Caller: invokes defragalloc (extracted from defragfns->defragAlloc) */
static void dictDefragBucket(dict *d, dictEntry **bucketref, dictDefragFunctions *defragfns) {
    dictDefragAllocFunction *defragalloc = defragfns->defragAlloc;
    dictDefragAllocFunction *defragkey = defragfns->defragKey;
    dictDefragAllocFunction *defragval = defragfns->defragVal;
    while (bucketref && *bucketref) {
        dictEntry *de = *bucketref, *newde = NULL;
        void *newkey = defragkey ? defragkey(dictGetKey(de)) : NULL;
        void *newval = defragval ? defragval(dictGetVal(de)) : NULL;
        if (entryIsKey(de)) {
            if (newkey) *bucketref = newkey;
        } else if (entryIsNoValue(de)) {
            dictEntryNoValue *entry = decodeEntryNoValue(de), *newentry;
            if ((newentry = defragalloc(entry))) {
                newde = encodeMaskPtr(newentry, ENTRY_PTR_NO_VALUE);
                entry = newentry;
            }
        }
        break;
    }
}

unsigned long dictScanDefrag(dict *d,
                             unsigned long v,
                             dictScanFunction *fn,
                             dictDefragFunctions *defragfns,
                             void *privdata)
{
    (void)fn; (void)privdata;
    if (!d->ht_table[0].rehashidx) {
        if (defragfns) {
            dictDefragBucket(d, (dictEntry **)&d->ht_table[0].ht_table[v & 0xff], defragfns);
        }
    } else {
        if (defragfns) {
            dictDefragBucket(d, (dictEntry **)&d->ht_table[0].ht_table[v & 0xff], defragfns);
        }
        if (defragfns) {
            dictDefragBucket(d, (dictEntry **)&d->ht_table[1].ht_table[v & 0xff], defragfns);
        }
    }
    return v;
}

typedef struct robj robj;
typedef struct client client;
enum { OBJ_ZSET, OBJ_SET, OBJ_HASH, OBJ_ENCODING_SKIPLIST, OBJ_ENCODING_HT };

void activeDefragCycle(void) {
    dict *db_dict = NULL;
    dictDefragFunctions defragfns = {.defragAlloc = activeDefragAlloc};
    unsigned long cursor = 0, expires_cursor = 0;
    dictScanFunction *defragScanCallback = NULL;
    dictScanFunction *scanCallbackCountScanned = NULL;

    cursor = dictScanDefrag(db_dict, cursor, defragScanCallback,
                            &defragfns, NULL);
    if (!cursor)
        expires_cursor = dictScanDefrag(NULL, expires_cursor,
                                        scanCallbackCountScanned,
                                        &defragfns, NULL);
}

void scanLaterZset(robj *ob, unsigned long *cursor) {
    (void)ob; (void)cursor;
}

void scanLaterSet(robj *ob, unsigned long *cursor) {
    (void)ob; (void)cursor;
}

void scanLaterHash(robj *ob, unsigned long *cursor) {
    (void)ob; (void)cursor;
}

unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       void *privdata)
{
    return dictScanDefrag(d, v, fn, NULL, privdata);
}
