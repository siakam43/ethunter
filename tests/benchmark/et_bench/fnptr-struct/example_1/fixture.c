/* ET-Bench fixture: fnptr-struct/example_1 */
/* Scenario: Redis zrange result handler vtable.
   fnptr: handler->finalizeResultEmission
   targets: zrangeResultFinalizeClient, zrangeResultFinalizeStore
   caller: genericZrangebyrankCommand */

#include <stddef.h>
#include <string.h>

/* Forward declarations for opaque types */
typedef struct client client;
typedef struct robj robj;

enum zrange_consumer_type {
    ZRANGE_CONSUMER_TYPE_CLIENT,
    ZRANGE_CONSUMER_TYPE_INTERNAL
};

enum zrange_type {
    ZRANGE_AUTO,
    ZRANGE_RANK,
    ZRANGE_SCORE,
    ZRANGE_LEX
};

enum zrange_direction {
    ZRANGE_DIRECTION_AUTO,
    ZRANGE_DIRECTION_REVERSE
};

typedef void (*result_begin_fn)(struct zrange_result_handler *h, long length);
typedef void (*result_finalize_fn)(struct zrange_result_handler *h, size_t count);
typedef void (*result_emit_buf_fn)(struct zrange_result_handler *h, const void *buf, size_t len, double score);
typedef void (*result_emit_long_fn)(struct zrange_result_handler *h, long long value, double score);

typedef struct zrange_result_handler {
    client *client;
    int withscores;
    int should_emit_array_length;
    void *userdata;
    robj *dstkey;
    robj *dstobj;
    result_begin_fn beginResultEmission;
    result_finalize_fn finalizeResultEmission;
    result_emit_buf_fn emitResultFromCBuffer;
    result_emit_long_fn emitResultFromLongLong;
} zrange_result_handler;

static long llen = 0;

/* Target: Client-side finalize */
static void zrangeResultFinalizeClient(zrange_result_handler *handler,
    size_t result_count)
{
    if (!handler->userdata)
        return;
    if (handler->withscores) {
        result_count *= 2;
    }
    /* Set deferred array length for the client response */
    handler->userdata = NULL;
}

/* Target: Store-side finalize */
static void zrangeResultFinalizeStore(zrange_result_handler *handler, size_t result_count)
{
    if (result_count) {
        /* Store result in destination key */
        handler->dstkey = NULL;
    } else {
        /* Delete destination key if no results */
    }
    handler->dstobj = NULL;
}

static void zrangeResultBeginClient(zrange_result_handler *handler, long length) {
    if (length > 0) {
        if (handler->withscores) {
            length *= 2;
        }
        handler->userdata = NULL;
        return;
    }
    handler->userdata = (void *)0;
}

static void zrangeResultBeginStore(zrange_result_handler *handler, long length)
{
    (void)length;
    handler->dstobj = NULL;
}

static void zrangeResultEmitCBufferToClient(zrange_result_handler *handler,
    const void *value, size_t value_length_in_bytes, double score)
{
    (void)value; (void)value_length_in_bytes; (void)score;
}

static void zrangeResultEmitLongLongToClient(zrange_result_handler *handler,
    long long value, double score)
{
    (void)value; (void)score;
}

static void zrangeResultEmitCBufferForStore(zrange_result_handler *handler,
    const void *value, size_t value_length_in_bytes, double score)
{
    (void)value; (void)value_length_in_bytes; (void)score;
}

static void zrangeResultEmitLongLongForStore(zrange_result_handler *handler,
    long long value, double score)
{
    (void)value; (void)score;
}

static void zrangeResultHandlerInit(zrange_result_handler *handler,
    client *client, enum zrange_consumer_type type)
{
    memset(handler, 0, sizeof(*handler));
    handler->client = client;

    switch (type) {
    case ZRANGE_CONSUMER_TYPE_CLIENT:
        handler->beginResultEmission = zrangeResultBeginClient;
        handler->finalizeResultEmission = zrangeResultFinalizeClient;
        handler->emitResultFromCBuffer = zrangeResultEmitCBufferToClient;
        handler->emitResultFromLongLong = zrangeResultEmitLongLongToClient;
        break;

    case ZRANGE_CONSUMER_TYPE_INTERNAL:
        handler->beginResultEmission = zrangeResultBeginStore;
        handler->finalizeResultEmission = zrangeResultFinalizeStore;
        handler->emitResultFromCBuffer = zrangeResultEmitCBufferForStore;
        handler->emitResultFromLongLong = zrangeResultEmitLongLongForStore;
        break;
    }
}

/* Caller: invokes handler->finalizeResultEmission through the struct */
void genericZrangebyrankCommand(zrange_result_handler *handler,
    robj *zobj, long start, long end, int withscores, int reverse) {
    (void)zobj; (void)withscores; (void)reverse;
    if (start > end || start >= llen) {
        handler->beginResultEmission(handler, 0);
        handler->finalizeResultEmission(handler, 0);
        return;
    }
}

void genericZrangebyscoreCommand(zrange_result_handler *handler, void *range,
    robj *zobj, long offset, long limit, int reverse) {
    (void)handler; (void)range; (void)zobj; (void)offset; (void)limit; (void)reverse;
}

void genericZrangebylexCommand(zrange_result_handler *handler, void *lexrange,
    robj *zobj, int withscores, long offset, long limit, int reverse) {
    (void)handler; (void)lexrange; (void)zobj; (void)withscores;
    (void)offset; (void)limit; (void)reverse;
}

void zrangeGenericCommand(zrange_result_handler *handler, int argc_start, int store,
                          enum zrange_type rangetype, enum zrange_direction direction)
{
    robj *zobj = NULL;
    long opt_start = 0, opt_end = 0;
    int opt_withscores = 0;
    long opt_offset = 0, opt_limit = 0;
    void *range = NULL;
    void *lexrange = NULL;

    switch (rangetype) {
    case ZRANGE_AUTO:
    case ZRANGE_RANK:
        genericZrangebyrankCommand(handler, zobj, opt_start, opt_end,
            opt_withscores || store, direction == ZRANGE_DIRECTION_REVERSE);
        break;

    case ZRANGE_SCORE:
        genericZrangebyscoreCommand(handler, range, zobj, opt_offset,
            opt_limit, direction == ZRANGE_DIRECTION_REVERSE);
        break;

    case ZRANGE_LEX:
        genericZrangebylexCommand(handler, lexrange, zobj, opt_withscores || store,
            opt_offset, opt_limit, direction == ZRANGE_DIRECTION_REVERSE);
        break;
    }
}

void zrangeResultHandlerDestinationKeySet(zrange_result_handler *handler, robj *dstkey) {
    handler->dstkey = dstkey;
}

void zrangestoreCommand(client *c) {
    robj *dstkey = NULL;
    zrange_result_handler handler;
    zrangeResultHandlerInit(&handler, c, ZRANGE_CONSUMER_TYPE_INTERNAL);
    zrangeResultHandlerDestinationKeySet(&handler, dstkey);
    zrangeGenericCommand(&handler, 2, 1, ZRANGE_AUTO, ZRANGE_DIRECTION_AUTO);
}
