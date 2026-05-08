/* CG-Bench fixture: fnptr-library/example_1 */
/* fnptr: c->funcs->read, targets: redisNetRead */

/* Read raw bytes through a redisContext. The read operation is not greedy
 * and may not fill the buffer entirely.
 */
static ssize_t readConn(redisContext *c, char *buf, size_t len)
{
    return c->funcs->read(c, buf, len);
}

static redisContext *context;

static int cliConnect(int flags) {
    if (context == NULL || flags & CC_FORCE) {
        if (context != NULL) {
            redisFree(context);
            config.dbnum = 0;
            config.in_multi = 0;
            config.pubsub_mode = 0;
            cliRefreshPrompt();
        }

        /* Do not use hostsocket when we got redirected in cluster mode */
        if (config.hostsocket == NULL ||
            (config.cluster_mode && config.cluster_reissue_command)) {
            context = redisConnect(config.conn_info.hostip,config.conn_info.hostport);
        } else {
            context = redisConnectUnix(config.hostsocket);
        }
    }
}

/* Connect to a Redis instance. On error the field error in the returned
 * context will be set to the return value of the error function.
 * When no set of reply functions is given, the default set will be used. */
redisContext *redisConnect(const char *ip, int port) {
    redisOptions options = {0};
    REDIS_OPTIONS_SET_TCP(&options, ip, port);
    return redisConnectWithOptions(&options);
}

redisContext *redisConnectWithOptions(const redisOptions *options) {
    redisContext *c = redisContextInit();
    if (c == NULL) {
        return NULL;
    }
}

static redisContext *redisContextInit(void) {
    redisContext *c;

    c = hi_calloc(1, sizeof(*c));
    if (c == NULL)
        return NULL;

    c->funcs = &redisContextDefaultFuncs;

    c->obuf = hi_sdsempty();
    c->reader = redisReaderCreate();
    c->fd = REDIS_INVALID_FD;

    if (c->obuf == NULL || c->reader == NULL) {
        redisFree(c);
        return NULL;
    }

    return c;
}

static redisContextFuncs redisContextDefaultFuncs = {
    .close = redisNetClose,
    .free_privctx = NULL,
    .async_read = redisAsyncRead,
    .async_write = redisAsyncWrite,
    .read = redisNetRead,
    .write = redisNetWrite
};


/* Stub implementation for redisNetRead */
void redisNetRead(void) {}
