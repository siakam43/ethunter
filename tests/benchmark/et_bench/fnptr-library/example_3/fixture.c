/* ET-Bench fixture: fnptr-library/example_3 */
/* fnptr: sock.read, targets: ssl_read, sock_read */
/* Pattern: library context with connection type struct containing function pointers */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct connection {
    char *buf;
    size_t buf_len;
} connection;

typedef enum { CONN_OK, CONN_ERROR, CONN_RETRY } conn_status;

typedef struct sock {
    conn_status (*connect)(connection *c);
    conn_status (*close)(connection *c);
    conn_status (*read)(connection *c, size_t *n);
    conn_status (*write)(connection *c, const char *buf, size_t len);
    conn_status (*readable)(connection *c);
} sock_t;

/* HTTP parser stub */
typedef struct http_parser_settings { int dummy; } http_parser_settings_t;
static http_parser_settings_t parser_settings;

static size_t http_parser_execute(void *parser, void *settings,
                                  const char *buf, size_t n) {
    (void)parser; (void)settings; (void)buf;
    return n;
}
static int http_body_is_final(void *parser) { (void)parser; return 1; }

static struct sock sock;

static void *ssl_init(void) { return NULL; }
static conn_status ssl_connect(connection *c) { (void)c; return CONN_OK; }
static conn_status ssl_close(connection *c) { (void)c; return CONN_OK; }
static conn_status ssl_readable(connection *c) { (void)c; return CONN_OK; }
static conn_status sock_connect(connection *c) { (void)c; return CONN_OK; }
static conn_status sock_close(connection *c) { (void)c; return CONN_OK; }
static conn_status sock_write(connection *c, const char *buf, size_t len) {
    (void)c; (void)buf; (void)len; return CONN_OK;
}
static conn_status sock_readable(connection *c) { (void)c; return CONN_OK; }

static void socket_readable(void *loop, int fd, void *data, int mask) {
    connection *c = (connection *)data;
    size_t n;
    conn_status status;

    do {
        status = sock.read(c, &n);
        switch (status) {
            case CONN_OK:    break;
            case CONN_ERROR: return;
            case CONN_RETRY: return;
        }

        if (http_parser_execute(NULL, &parser_settings, c->buf, n) != n)
            return;
        if (n == 0 && !http_body_is_final(NULL))
            return;
    } while (0);
}

/* Target 1: default socket read */
static conn_status sock_read(connection *c, size_t *n) {
    (void)c;
    *n = 0;
    return CONN_OK;
}

/* Target 2: SSL read */
static conn_status ssl_read(connection *c, size_t *n) {
    (void)c;
    *n = 0;
    return CONN_OK;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    sock.connect  = sock_connect;
    sock.close    = sock_close;
    sock.read     = sock_read;
    sock.write    = sock_write;
    sock.readable = sock_readable;

    /* In an HTTPS configuration, read would be ssl_read */
    sock.read = ssl_read;

    return 0;
}
