/* ET-Bench fixture: fnptr-library/example_1 */
/* fnptr: c->funcs->read, targets: redisNetRead */
/* Pattern: library context with funcs struct containing function pointers, called through context */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct redisContextFuncs {
    void (*close)(void *);
    void (*free_privctx)(void *);
    void (*async_read)(void *);
    void (*async_write)(void *);
    ssize_t (*read)(void *c, char *buf, size_t len);
    ssize_t (*write)(void *c, const char *buf, size_t len);
} redisContextFuncs;

typedef struct redisContext {
    redisContextFuncs *funcs;
    void *obuf;
    void *reader;
    int fd;
} redisContext;

static redisContextFuncs redisContextDefaultFuncs = {
    .close = NULL,
    .free_privctx = NULL,
    .async_read = NULL,
    .async_write = NULL,
    .read = redisNetRead,
    .write = NULL
};

static void *hi_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

static void *hi_sdsempty(void) { return NULL; }

static redisContext *redisContextInit(void) {
    redisContext *c;

    c = (redisContext *)hi_calloc(1, sizeof(*c));
    if (c == NULL)
        return NULL;

    c->funcs = &redisContextDefaultFuncs;
    c->obuf = hi_sdsempty();
    c->reader = NULL;
    c->fd = -1;

    return c;
}

redisContext *redisConnect(const char *ip, int port) {
    redisContext *c = redisContextInit();
    (void)ip; (void)port;
    return c;
}

/* Read raw bytes through a redisContext. The read operation is not greedy
 * and may not fill the buffer entirely.
 */
static ssize_t readConn(redisContext *c, char *buf, size_t len)
{
    return c->funcs->read(c, buf, len);
}

int cliConnect(int flags) {
    redisContext *context = NULL;
    if (context != NULL) {
        free(context);
    }
    context = redisConnect("127.0.0.1", 6379);
    if (context == NULL)
        return -1;
    return 0;
}

/* Target: set in redisContextDefaultFuncs */
ssize_t redisNetRead(void *c, char *buf, size_t len) {
    (void)c; (void)buf; (void)len;
    return 0;
}
