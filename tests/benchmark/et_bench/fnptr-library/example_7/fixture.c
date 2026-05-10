/* ET-Bench fixture: fnptr-library/example_7 */
/* fnptr: conn->type->set_write_handler, targets: connSocketSetWriteHandler, connTLSSetWriteHandler, connUnixSetWriteHandler */
/* Pattern: library connection type struct with function pointers for different transport types */

#include <stdio.h>
#include <stdlib.h>

typedef struct connection Connection;
typedef void (*ConnectionCallbackFunc)(Connection *, int);

typedef struct ConnectionType {
    int (*write)(Connection *conn, const char *buf, size_t buf_len);
    int (*writev)(Connection *conn, struct iovec *iov, int iov_count);
    int (*read)(Connection *conn, char *buf, size_t buf_len);
    int (*set_write_handler)(Connection *conn, ConnectionCallbackFunc func, int barrier);
    int (*set_read_handler)(Connection *conn, ConnectionCallbackFunc func, int barrier);
    int (*get_last_error)(Connection *conn);
    int (*sync_write)(Connection *conn, char *buf, size_t buf_len, long long timeout);
    int (*sync_read)(Connection *conn, char *buf, size_t buf_len, long long timeout);
    int (*sync_readline)(Connection *conn, char *buf, size_t buf_len, long long timeout);
} ConnectionType;

struct connection {
    ConnectionType *type;
    int fd;
};

static inline int connSetWriteHandlerWithBarrier(Connection *conn, ConnectionCallbackFunc func, int barrier) {
    return conn->type->set_write_handler(conn, func, barrier);
}

/* Socket connection type handlers */
static int connSocketSetWriteHandler(Connection *conn, ConnectionCallbackFunc func, int barrier) {
    (void)conn; (void)func; (void)barrier;
    return 0;
}
static int connSocketRead(Connection *conn, char *buf, size_t len) { (void)conn; (void)buf; (void)len; return 0; }
static int connSocketWrite(Connection *conn, const char *buf, size_t len) { (void)conn; (void)buf; (void)len; return 0; }
static int connSocketWritev(Connection *conn, struct iovec *iov, int cnt) { (void)conn; (void)iov; (void)cnt; return 0; }
static int connSocketSetReadHandler(Connection *conn, ConnectionCallbackFunc func, int barrier) { (void)conn; (void)func; (void)barrier; return 0; }
static int connSocketGetLastError(Connection *conn) { (void)conn; return 0; }
static int connSocketSyncWrite(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }
static int connSocketSyncRead(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }
static int connSocketSyncReadLine(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }

/* TLS connection type handlers */
static int connTLSSetWriteHandler(Connection *conn, ConnectionCallbackFunc func, int barrier) {
    (void)conn; (void)func; (void)barrier;
    return 0;
}
static int connTLSRead(Connection *conn, char *buf, size_t len) { (void)conn; (void)buf; (void)len; return 0; }
static int connTLSWrite(Connection *conn, const char *buf, size_t len) { (void)conn; (void)buf; (void)len; return 0; }
static int connTLSWritev(Connection *conn, struct iovec *iov, int cnt) { (void)conn; (void)iov; (void)cnt; return 0; }
static int connTLSSetReadHandler(Connection *conn, ConnectionCallbackFunc func, int barrier) { (void)conn; (void)func; (void)barrier; return 0; }
static int connTLSGetLastError(Connection *conn) { (void)conn; return 0; }
static int connTLSSyncWrite(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }
static int connTLSSyncRead(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }
static int connTLSSyncReadLine(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }

/* Unix domain socket connection type handlers */
static int connUnixSetWriteHandler(Connection *conn, ConnectionCallbackFunc func, int barrier) {
    (void)conn; (void)func; (void)barrier;
    return 0;
}
static int connUnixRead(Connection *conn, char *buf, size_t len) { (void)conn; (void)buf; (void)len; return 0; }
static int connUnixWrite(Connection *conn, const char *buf, size_t len) { (void)conn; (void)buf; (void)len; return 0; }
static int connUnixWritev(Connection *conn, struct iovec *iov, int cnt) { (void)conn; (void)iov; (void)cnt; return 0; }
static int connUnixSetReadHandler(Connection *conn, ConnectionCallbackFunc func, int barrier) { (void)conn; (void)func; (void)barrier; return 0; }
static int connUnixGetLastError(Connection *conn) { (void)conn; return 0; }
static int connUnixSyncWrite(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }
static int connUnixSyncRead(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }
static int connUnixSyncReadLine(Connection *c, char *b, size_t l, long long t) { (void)c; (void)b; (void)l; (void)t; return 0; }

static ConnectionType CT_Socket = {
    .write = connSocketWrite,
    .writev = connSocketWritev,
    .read = connSocketRead,
    .set_write_handler = connSocketSetWriteHandler,
    .set_read_handler = connSocketSetReadHandler,
    .get_last_error = connSocketGetLastError,
    .sync_write = connSocketSyncWrite,
    .sync_read = connSocketSyncRead,
    .sync_readline = connSocketSyncReadLine,
};

static ConnectionType CT_TLS = {
    .write = connTLSWrite,
    .writev = connTLSWritev,
    .read = connTLSRead,
    .set_write_handler = connTLSSetWriteHandler,
    .set_read_handler = connTLSSetReadHandler,
    .get_last_error = connTLSGetLastError,
    .sync_write = connTLSSyncWrite,
    .sync_read = connTLSSyncRead,
    .sync_readline = connTLSSyncReadLine,
};

static ConnectionType CT_Unix = {
    .write = connUnixWrite,
    .writev = connUnixWritev,
    .read = connUnixRead,
    .set_write_handler = connUnixSetWriteHandler,
    .set_read_handler = connUnixSetReadHandler,
    .get_last_error = connUnixGetLastError,
    .sync_write = connUnixSyncWrite,
    .sync_read = connUnixSyncRead,
    .sync_readline = connUnixSyncReadLine,
};
