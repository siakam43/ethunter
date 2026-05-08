/* CG-Bench fixture: fnptr-global-struct/example_5 */
/* fnptr: connectionTypeTcp()->read, targets: connTLSRead */

static int connUnixRead(connection *conn, void *buf, size_t buf_len) {
    return connectionTypeTcp()->read(conn, buf, buf_len);
}

ConnectionType *connectionByType(const char *typename) {
    ConnectionType *ct;

    for (int type = 0; type < CONN_TYPE_MAX; type++) {
        ct = connTypes[type];
        if (!ct)
            break;

        if (!strcasecmp(typename, ct->get_type(NULL)))
            return ct;
    }

    serverLog(LL_WARNING, "Missing implement of connection type %s", typename);

    return NULL;
}

/* Cache TCP connection type, query it by string once */
ConnectionType *connectionTypeTcp(void) {
    static ConnectionType *ct_tcp = NULL;

    if (ct_tcp != NULL)
        return ct_tcp;

    ct_tcp = connectionByType(CONN_TYPE_SOCKET);
    serverAssert(ct_tcp != NULL);

    return ct_tcp;
}

static ConnectionType *connTypes[CONN_TYPE_MAX];

int connTypeRegister(ConnectionType *ct) {
    ...
    serverLog(LL_VERBOSE, "Connection type %s registered", typename);
    connTypes[type] = ct;
    ...
    return C_OK;
}

int RedisRegisterConnectionTypeSocket(void)
{
    return connTypeRegister(&CT_Socket);
}

int RedisRegisterConnectionTypeTLS(void) {
    return connTypeRegister(&CT_TLS);
}

static ConnectionType CT_Socket = {
   ...
    .read = connTLSRead,
    .write = connTLSWrite,
    .writev = connTLSWritev,
    ...
};

static ConnectionType CT_TLS = {
    ...

    .read = connTLSRead,
    .write = connTLSWrite,
    .writev = connTLSWritev,
    ...
}


/* Stub implementation for connTLSRead */
void connTLSRead(void) {}
