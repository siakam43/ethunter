/* CG-Bench fixture: fnptr-struct/example_6 */
/* fnptr: defragalloc, targets: activeDefragAlloc */

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
            assert(entryIsKey(*bucketref));
        } else if (entryIsNoValue(de)) {
            dictEntryNoValue *entry = decodeEntryNoValue(de), *newentry;
            if ((newentry = defragalloc(entry))) {
                newde = encodeMaskedPtr(newentry, ENTRY_PTR_NO_VALUE);
                entry = newentry;
            }
        }
    }
}

unsigned long dictScanDefrag(dict *d,
                             unsigned long v,
                             dictScanFunction *fn,
                             dictDefragFunctions *defragfns,
                             void *privdata)
{
    ...
    if (!dictIsRehashing(d)) {
        ...

        /* Emit entries at cursor */
        if (defragfns) {
            dictDefragBucket(d, &d->ht_table[htidx0][v & m0], defragfns);
        }
        ...
    } else {
        ...
        if (defragfns) {
            dictDefragBucket(d, &d->ht_table[htidx0][v & m0], defragfns);
        }
        ...
        do {
            if (defragfns) {
                dictDefragBucket(d, &d->ht_table[htidx1][v & m1], defragfns);
            }
            ...
        } while (v & (m0 ^ m1));
    }
    return v;
}

void activeDefragCycle(void) {
    ...

    dictDefragFunctions defragfns = {.defragAlloc = activeDefragAlloc};
    do {
        do {
            ...

            /* Scan the keyspace dict unless we're scanning the expire dict. */
            if (!expires_cursor)
                cursor = dictScanDefrag(db->dict, cursor, defragScanCallback,
                                        &defragfns, db);

            /* When done scanning the keyspace dict, we scan the expire dict. */
            if (!cursor)
                expires_cursor = dictScanDefrag(db->expires, expires_cursor,
                                                scanCallbackCountScanned,
                                                &defragfns, NULL);
            ...
        }
    }
}

void scanLaterZset(robj *ob, unsigned long *cursor) {
    if (ob->type != OBJ_ZSET || ob->encoding != OBJ_ENCODING_SKIPLIST)
        return;
    zset *zs = (zset*)ob->ptr;
    dict *d = zs->dict;
    scanLaterZsetData data = {zs};
    dictDefragFunctions defragfns = {.defragAlloc = activeDefragAlloc};
    *cursor = dictScanDefrag(d, *cursor, scanLaterZsetCallback, &defragfns, &data);
}

void scanLaterSet(robj *ob, unsigned long *cursor) {
    if (ob->type != OBJ_SET || ob->encoding != OBJ_ENCODING_HT)
        return;
    dict *d = ob->ptr;
    dictDefragFunctions defragfns = {
        .defragAlloc = activeDefragAlloc,
        .defragKey = (dictDefragAllocFunction *)activeDefragSds
    };
    *cursor = dictScanDefrag(d, *cursor, scanCallbackCountScanned, &defragfns, NULL);
}

void scanLaterHash(robj *ob, unsigned long *cursor) {
    if (ob->type != OBJ_HASH || ob->encoding != OBJ_ENCODING_HT)
        return;
    dict *d = ob->ptr;
    dictDefragFunctions defragfns = {
        .defragAlloc = activeDefragAlloc,
        .defragKey = (dictDefragAllocFunction *)activeDefragSds,
        .defragVal = (dictDefragAllocFunction *)activeDefragSds
    };
    *cursor = dictScanDefrag(d, *cursor, scanCallbackCountScanned, &defragfns, NULL);
}

unsigned long dictScan(dict *d,
                       unsigned long v,
                       dictScanFunction *fn,
                       void *privdata)
{
    return dictScanDefrag(d, v, fn, NULL, privdata);
}


/* Stub implementation for activeDefragAlloc */
void activeDefragAlloc(void) {}
