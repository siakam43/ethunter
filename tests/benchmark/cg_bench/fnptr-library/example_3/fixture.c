/* CG-Bench fixture: fnptr-library/example_3 */
/* fnptr: sock.read, targets: ssl_read, sock_read */

static void socket_readable(aeEventLoop *loop, int fd, void *data, int mask) {
connection *c = data;
    size_t n;

    do {
        switch (sock.read(c, &n)) {
            case OK:    break;
            case ERROR: goto error;
            case RETRY: return;
        }

        if (http_parser_execute(&c->parser, &parser_settings, c->buf, n) != n) goto error;
        if (n == 0 && !http_body_is_final(&c->parser)) goto error;
    }
}

int main(int argc, char **argv) {
    char *url, **headers = zmalloc(argc * sizeof(char *));
    struct http_parser_url parts = {};

    ...

    if (!strncmp("https", schema, 5)) {
        if ((cfg.ctx = ssl_init()) == NULL) {
            fprintf(stderr, "unable to initialize SSL\n");
            ERR_print_errors_fp(stderr);
            exit(1);
        }
        sock.connect  = ssl_connect;
        sock.close    = ssl_close;
        sock.read     = ssl_read;
        sock.write    = ssl_write;
        sock.readable = ssl_readable;
    }
}
...

static struct sock sock = {
.connect  = sock_connect,
    .close    = sock_close,
    .read     = sock_read,
    .write    = sock_write,
    .readable = sock_readable
};


/* Stub implementation for ssl_read */
void ssl_read(void) {}



/* Stub implementation for sock_read */
void sock_read(void) {}
