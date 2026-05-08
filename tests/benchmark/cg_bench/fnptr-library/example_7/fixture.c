/* CG-Bench fixture: fnptr-library/example_7 */
/* fnptr: conn->type->set_write_handler, targets: connSocketSetWriteHandler, connTLSSetWriteHandler, connUnixSetWriteHandler */

static inline int connSetWriteHandlerWithBarrier(connection *conn, ConnectionCallbackFunc func, int barrier) {
    return conn->type->set_write_handler(conn, func, barrier);
}

static ConnectionType CT_Socket = {
    ...
    .write = connSocketWrite,
    .writev = connSocketWritev,
    .read = connSocketRead,
    .set_write_handler = connSocketSetWriteHandler,
    .set_read_handler = connSocketSetReadHandler,
    .get_last_error = connSocketGetLastError,
    .sync_write = connSocketSyncWrite,
    .sync_read = connSocketSyncRead,
    .sync_readline = connSocketSyncReadLine,
    ...
};

static ConnectionType CT_TLS = {
    ...
    .read = connTLSRead,
    .write = connTLSWrite,
    .writev = connTLSWritev,
    .set_write_handler = connTLSSetWriteHandler,
    .set_read_handler = connTLSSetReadHandler,
    .get_last_error = connTLSGetLastError,
    .sync_write = connTLSSyncWrite,
    .sync_read = connTLSSyncRead,
    .sync_readline = connTLSSyncReadLine,
    ...
};

static ConnectionType CT_Unix = {
    ...
    .write = connUnixWrite,
    .writev = connUnixWritev,
    .read = connUnixRead,
    .set_write_handler = connUnixSetWriteHandler,
    .set_read_handler = connUnixSetReadHandler,
    .get_last_error = connUnixGetLastError,
    .sync_write = connUnixSyncWrite,
    .sync_read = connUnixSyncRead,
    .sync_readline = connUnixSyncReadLine,
    ...
};


/* Stub implementation for connSocketSetWriteHandler */
void connSocketSetWriteHandler(void) {}



/* Stub implementation for connTLSSetWriteHandler */
void connTLSSetWriteHandler(void) {}



/* Stub implementation for connUnixSetWriteHandler */
void connUnixSetWriteHandler(void) {}
