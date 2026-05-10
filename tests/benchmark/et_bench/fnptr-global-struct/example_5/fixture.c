/* et_bench fixture: fnptr-global-struct/example_5 */
/* fnptr: connectionTypeTcp()->read, targets: connTLSRead */

#include <stddef.h>
#include <string.h>

#define CONN_TYPE_MAX 4
#define CONN_TYPE_SOCKET "socket"
#define C_OK 0

typedef struct connection connection;
typedef struct ConnectionType ConnectionType;

struct connection {
    int fd;
};

struct ConnectionType {
    const char *(*get_type)(const char *);
    int (*read)(connection *conn, void *buf, size_t buf_len);
    int (*write)(connection *conn, const void *buf, size_t buf_len);
    int (*writev)(connection *conn, const struct iovec *iov, int iovcnt);
};

void serverLog(int level, const char *fmt, ...) {}
void serverAssert(int cond) {}

/* Forward decls */
int connTLSRead(connection *conn, void *buf, size_t buf_len);
int connTLSWrite(connection *conn, const void *buf, size_t buf_len);
int connTLSWritev(connection *conn, const struct iovec *iov, int iovcnt);
static ConnectionType *connTypes[CONN_TYPE_MAX];

/* Registration */
int connTypeRegister(ConnectionType *ct)
{
    const char *typename = ct->get_type(NULL);
    for (int type = 0; type < CONN_TYPE_MAX; type++) {
        if (!connTypes[type]) {
            connTypes[type] = ct;
            serverLog(0, "Connection type %s registered", typename);
            return C_OK;
        }
    }
    return -1;
}

int RedisRegisterConnectionTypeSocket(void)
{
    return connTypeRegister(&CT_Socket);
}

int RedisRegisterConnectionTypeTLS(void)
{
    return connTypeRegister(&CT_TLS);
}

/* Cache TCP connection type, query it by string once */
ConnectionType *connectionTypeTcp(void)
{
    static ConnectionType *ct_tcp = NULL;
    if (ct_tcp != NULL)
        return ct_tcp;
    for (int type = 0; type < CONN_TYPE_MAX; type++) {
        if (connTypes[type] &&
            strcmp(connTypes[type]->get_type(NULL), CONN_TYPE_SOCKET) == 0) {
            ct_tcp = connTypes[type];
            return ct_tcp;
        }
    }
    serverAssert(ct_tcp != NULL);
    return NULL;
}

int connUnixRead(connection *conn, void *buf, size_t buf_len)
{
    return connectionTypeTcp()->read(conn, buf, buf_len);
}

/* Connection type implementations */
const char *socket_get_type(const char *unused) { return CONN_TYPE_SOCKET; }
const char *tls_get_type(const char *unused) { return "tls"; }

int connTLSRead(connection *conn, void *buf, size_t buf_len)
{
    return 0;
}

int connTLSWrite(connection *conn, const void *buf, size_t buf_len)
{
    return 0;
}

int connTLSWritev(connection *conn, const struct iovec *iov, int iovcnt)
{
    return 0;
}

static ConnectionType CT_Socket = {
    .get_type = socket_get_type,
    .read = connTLSRead,
    .write = connTLSWrite,
    .writev = connTLSWritev,
};

static ConnectionType CT_TLS = {
    .get_type = tls_get_type,
    .read = connTLSRead,
    .write = connTLSWrite,
    .writev = connTLSWritev,
};
