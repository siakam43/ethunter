/* CG-Bench fixture: fnptr-callback/example_7 */
/* fnptr: element_cb, targets: defragStreamConsumerPendingEntry, defragStreamConsumer, defragStreamConsumerGroup */

void defragRadixTree(rax **raxref, int defrag_data, raxDefragFunction *element_cb, void *element_cb_data) {
    raxIterator ri;
    rax* rax;

    while (raxNext(&ri)) {
        void *newdata = NULL;
        if (element_cb)
            newdata = element_cb(&ri, element_cb_data);
        if (defrag_data && !newdata)
            newdata = activeDefragAlloc(ri.data);
        if (newdata)
            raxSetData(ri.node, ri.data=newdata);
    }
    raxStop(&ri);
}

void* defragStreamConsumer(raxIterator *ri, void *privdata) {
    streamConsumer *c = ri->data;
    streamCG *cg = privdata;
    void *newc = activeDefragAlloc(c);
    if (newc) {
        c = newc;
    }
    sds newsds = activeDefragSds(c->name);
    if (newsds)
        c->name = newsds;
    if (c->pel) {
        PendingEntryContext pel_ctx = {cg, c};
        defragRadixTree(&c->pel, 0, defragStreamConsumerPendingEntry, &pel_ctx);
    }
    return newc; /* returns NULL if c was not defragged */
}

void* defragStreamConsumerGroup(raxIterator *ri, void *privdata) {
    streamCG *cg = ri->data;
    UNUSED(privdata);
    if (cg->consumers)
        defragRadixTree(&cg->consumers, 0, defragStreamConsumer, cg);
    if (cg->pel)
        defragRadixTree(&cg->pel, 0, NULL, NULL);
    return NULL;
}

void defragStream(redisDb *db, dictEntry *kde) {
    robj *ob = dictGetVal(kde);
    serverAssert(ob->type == OBJ_STREAM && ob->encoding == OBJ_ENCODING_STREAM);
    stream *s = ob->ptr, *news;

    /* handle the main struct */
    if ((news = activeDefragAlloc(s)))
        ob->ptr = s = news;

    if (raxSize(s->rax) > server.active_defrag_max_scan_fields) {
        rax *newrax = activeDefragAlloc(s->rax);
        if (newrax)
            s->rax = newrax;
        defragLater(db, kde);
    } else
        defragRadixTree(&s->rax, 1, NULL, NULL);

    if (s->cgroups)
        defragRadixTree(&s->cgroups, 1, defragStreamConsumerGroup, NULL);
}


/* Stub implementation for defragStreamConsumerPendingEntry */
void defragStreamConsumerPendingEntry(void) {}
