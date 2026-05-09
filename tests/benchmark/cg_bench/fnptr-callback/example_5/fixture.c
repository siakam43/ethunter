/* CG-Bench fixture: fnptr-callback/example_5 */
/* fnptr: saver, targets: _quicklistSaver, listPopSaver */

int quicklistPopCustom(quicklist *quicklist, int where, unsigned char **data,
                       size_t *sz, long long *sval,
                       void *(*saver)(unsigned char *data, size_t sz)) {
    unsigned char *p;
    unsigned char *vstr;
    unsigned int vlen;
    long long vlong;
    int pos = (where == QUICKLIST_HEAD) ? 0 : -1;

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
    return ret;
}

/* Stub implementation for _quicklistSaver */
void _quicklistSaver(void) {}

/* Stub implementation for listPopSaver */
void listPopSaver(void) {}
