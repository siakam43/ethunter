/* CG-Bench fixture: fnptr-library/example_6 */
/* fnptr: mt->mem_usage2, targets: NULL */

size_t moduleGetMemUsage(robj *key, robj *val, size_t sample_size, int dbid) {
    moduleValue *mv = val->ptr;
    moduleType *mt = mv->type;
    size_t size = 0;
    /* We prefer to use the enhanced version. */
    if (mt->mem_usage2 != NULL) {
        RedisModuleKeyOptCtx ctx = {key, NULL, dbid, -1};
        size = mt->mem_usage2(&ctx, mv->value, sample_size);
    } else if (mt->mem_usage != NULL) {
        size = mt->mem_usage(mv->value);
    } 

    return size;
}

size_t objectComputeSize(robj *key, robj *o, size_t sample_size, int dbid) {
    sds ele, ele2;
    dict *d;
    dictIterator *di;
    struct dictEntry *de;
    size_t asize = 0, elesize = 0, samples = 0;

    if (o->type == OBJ_STRING) {
        ...
    } else if (o->type == OBJ_HASH) {
        ...
    } else if (o->type == OBJ_STREAM) {
        ...
    } else if (o->type == OBJ_MODULE) {
        asize = moduleGetMemUsage(key, o, sample_size, dbid);
    } else {
        serverPanic("Unknown object type");
    }
    return asize;
}

void memoryCommand(client *c) {
    if (!strcasecmp(c->argv[1]->ptr,"help") && c->argc == 2) {
        ...
    } else if (!strcasecmp(c->argv[1]->ptr,"usage") && c->argc >= 3) {
        dictEntry *de;
        ...
        size_t usage = objectComputeSize(c->argv[2],dictGetVal(de),samples,c->db->id);
        usage += sdsZmallocSize(dictGetKey(de));
        usage += dictEntryMemUsage();
        usage += dictMetadataSize(c->db->dict);
        addReplyLongLong(c,usage);
    } else if (!strcasecmp(c->argv[1]->ptr,"stats") && c->argc == 2) {
        ...
    }
}


/* Stub implementation for NULL */
void NULL(void) {}
