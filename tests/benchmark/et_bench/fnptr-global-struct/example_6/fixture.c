/* et_bench fixture: fnptr-global-struct/example_6 */
/* fnptr: r->fn->createDouble, targets: createDoubleObject */

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#define REDIS_OK   0
#define REDIS_ERR  -1
#define REDIS_READER_STACK_SIZE 9
#define REDIS_READER_MAX_BUF 16384
#define REDIS_READER_MAX_ARRAY_ELEMENTS (1ULL<<32)

#define REDIS_REPLY_STRING  1
#define REDIS_REPLY_ARRAY   2
#define REDIS_REPLY_INTEGER 3
#define REDIS_REPLY_DOUBLE  4
#define REDIS_REPLY_NIL     5
#define REDIS_REPLY_STATUS  6
#define REDIS_REPLY_ERROR   7
#define REDIS_REPLY_BOOL    8
#define REDIS_REPLY_MAP     9
#define REDIS_REPLY_SET    10
#define REDIS_REPLY_PUSH   11
#define REDIS_REPLY_VERB   12
#define REDIS_REPLY_BIGNUM 13

typedef struct redisReadTask {
    int type;
    int idx;
    int elements;
    int rid;
    size_t obj_type;
} redisReadTask;

typedef struct redisReplyObjectFunctions {
    void *(*createString)(const redisReadTask*, char*, size_t);
    void *(*createArray)(const redisReadTask*, size_t);
    void *(*createInteger)(const redisReadTask*, long long);
    void *(*createDouble)(const redisReadTask*, char*, size_t);
    void *(*createNil)(const redisReadTask*);
    void *(*createBool)(const redisReadTask*, int);
    void (*freeObject)(void*);
} redisReplyObjectFunctions;

typedef struct redisReader {
    char *buf;
    size_t len;
    size_t pos;
    size_t maxbuf;
    size_t maxelements;
    redisReadTask *task[REDIS_READER_STACK_SIZE];
    int tasks;
    int ridx;
    redisReplyObjectFunctions *fn;
} redisReader;

static void *hi_calloc(size_t nmemb, size_t size) { return calloc(nmemb, size); }
static void hi_free(void *p) { free(p); }
static char *hi_sdsempty(void) { return strdup(""); }

void *createStringObject(const redisReadTask *t, char *s, size_t l) { return s; }
void *createArrayObject(const redisReadTask *t, size_t e) { return NULL; }
void *createIntegerObject(const redisReadTask *t, long long v) { return NULL; }
void createDoubleObject(const redisReadTask *t, char *d, size_t l) {}
void *createNilObject(const redisReadTask *t) { return NULL; }
void *createBoolObject(const redisReadTask *t, int b) { return NULL; }
void freeReplyObject(void *o) {}

static char *readLine(redisReader *r, int *len) { return NULL; }

static int processLineItem(redisReader *r)
{
    redisReadTask *cur = r->task[r->ridx];
    void *obj;
    char *p;
    int len;

    if ((p = readLine(r, &len)) != NULL) {
        if (cur->type == REDIS_REPLY_INTEGER) {
            obj = r->fn->createInteger(cur, 0);
        } else if (cur->type == REDIS_REPLY_DOUBLE) {
            if (r->fn && r->fn->createDouble) {
                obj = r->fn->createDouble(cur, p, len);
            } else {
                obj = NULL;
            }
        } else if (cur->type == REDIS_REPLY_NIL) {
            obj = r->fn->createNil(cur);
        }
    }
    return REDIS_ERR;
}

static int processBulkItem(redisReader *r) { return REDIS_ERR; }
static int processAggregateItem(redisReader *r) { return REDIS_ERR; }

static int processItem(redisReader *r)
{
    redisReadTask *cur = r->task[r->ridx];

    switch (cur->type) {
    case REDIS_REPLY_ERROR:
    case REDIS_REPLY_STATUS:
    case REDIS_REPLY_INTEGER:
    case REDIS_REPLY_DOUBLE:
    case REDIS_REPLY_NIL:
    case REDIS_REPLY_BOOL:
    case REDIS_REPLY_BIGNUM:
        return processLineItem(r);
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_VERB:
        return processBulkItem(r);
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_MAP:
    case REDIS_REPLY_SET:
    case REDIS_REPLY_PUSH:
        return processAggregateItem(r);
    default:
        assert(0);
        return REDIS_ERR;
    }
}

int redisReaderGetReply(redisReader *r, void **reply)
{
    while (r->ridx >= 0)
        if (processItem(r) != REDIS_OK)
            break;
    return REDIS_OK;
}

void redisReaderFree(redisReader *r) { hi_free(r); }

redisReader *redisReaderCreateWithFunctions(redisReplyObjectFunctions *fn)
{
    redisReader *r;

    r = (redisReader *)hi_calloc(1, sizeof(redisReader));
    if (r == NULL)
        return NULL;

    r->buf = hi_sdsempty();
    if (r->buf == NULL)
        goto oom;

    for (int i = 0; i < REDIS_READER_STACK_SIZE; i++) {
        r->task[i] = (redisReadTask *)hi_calloc(1, sizeof(redisReadTask));
        if (r->task[i] == NULL)
            goto oom;
    }

    r->fn = fn;
    r->maxbuf = REDIS_READER_MAX_BUF;
    r->maxelements = REDIS_READER_MAX_ARRAY_ELEMENTS;
    r->ridx = -1;

    return r;
oom:
    redisReaderFree(r);
    return NULL;
}

redisReader *redisReaderCreate(void)
{
    return redisReaderCreateWithFunctions(&defaultFunctions);
}

redisReplyObjectFunctions defaultFunctions = {
    .createString = createStringObject,
    .createArray = createArrayObject,
    .createInteger = createIntegerObject,
    .createDouble = createDoubleObject,
    .createNil = createNilObject,
    .createBool = createBoolObject,
    .freeObject = freeReplyObject,
};
