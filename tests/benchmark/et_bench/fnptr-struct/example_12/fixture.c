/* ET-Bench fixture: fnptr-struct/example_12 */
/* Scenario: OpenSSL ALPN selection callback chain.
   fnptr: s->ctx->ext.alpn_select_cb
   target: alpn_cb
   caller: tls_handle_alpn */

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define OPENSSL_NPN_NEGOTIATED 1
#define SSL_TLSEXT_ERR_NOACK 1
#define SSL_TLSEXT_ERR_OK 0

typedef struct SSL SSL;
typedef struct SSL_CTX SSL_CTX;

typedef int (*SSL_CTX_alpn_select_cb_func)(SSL *s, const unsigned char **out,
                                           unsigned char *outlen,
                                           const unsigned char *in,
                                           unsigned int inlen, void *arg);

typedef struct ssl_ctx_ext {
    SSL_CTX_alpn_select_cb_func alpn_select_cb;
    void *alpn_select_cb_arg;
} ssl_ctx_ext;

struct SSL_CTX {
    ssl_ctx_ext ext;
};

typedef struct ssl3_state {
    const unsigned char *alpn_proposed;
    size_t alpn_proposed_len;
} ssl3_state;

struct SSL {
    SSL_CTX *ctx;
    ssl3_state *s3;
};

typedef struct tlsextalpnctx {
    const unsigned char *data;
    size_t len;
} tlsextalpnctx;

static FILE *bio_s_out = stdout;
static int s_quiet = 0;

static int BIO_printf(FILE *bio, const char *fmt, ...) {
    return fprintf(bio, "%s", fmt);
}

static int BIO_write(FILE *bio, const void *buf, int len) {
    size_t written = fwrite(buf, 1, len, bio);
    return (int)written;
}

static int SSL_select_next_proto(unsigned char **out, unsigned char *outlen,
                                 const unsigned char *server,
                                 unsigned int server_len,
                                 const unsigned char *client,
                                 unsigned int client_len)
{
    if (server_len == 0 || client_len == 0)
        return 0;
    *out = (unsigned char *)server;
    *outlen = server[0];
    return OPENSSL_NPN_NEGOTIATED;
}

/* Target: ALPN selection callback */
static int alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
                   const unsigned char *in, unsigned int inlen, void *arg)
{
    tlsextalpnctx *alpn_ctx = arg;

    if (!s_quiet) {
        unsigned int i;
        BIO_printf(bio_s_out, "ALPN protocols advertised by the client: ");
        for (i = 0; i < inlen;) {
            if (i)
                BIO_write(bio_s_out, ", ", 2);
            BIO_write(bio_s_out, &in[i + 1], in[i]);
            i += in[i] + 1;
        }
        BIO_write(bio_s_out, "\n", 1);
    }

    if (SSL_select_next_proto
        ((unsigned char **)out, outlen, alpn_ctx->data, alpn_ctx->len, in,
         inlen) != OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (!s_quiet) {
        BIO_printf(bio_s_out, "ALPN protocols selected: ");
        BIO_write(bio_s_out, *out, *outlen);
        BIO_write(bio_s_out, "\n", 1);
    }

    return SSL_TLSEXT_ERR_OK;
}

/* Caller: invokes s->ctx->ext.alpn_select_cb through the struct chain */
int tls_handle_alpn(SSL *s)
{
    const unsigned char *selected = NULL;
    unsigned char selected_len = 0;

    if (s->ctx->ext.alpn_select_cb != NULL && s->s3->alpn_proposed != NULL) {
        int r = s->ctx->ext.alpn_select_cb(s, &selected, &selected_len,
                                           s->s3->alpn_proposed,
                                           (unsigned int)s->s3->alpn_proposed_len,
                                           s->ctx->ext.alpn_select_cb_arg);
        if (r != SSL_TLSEXT_ERR_OK)
            return 0;
    }
    return 1;
}

void SSL_CTX_set_alpn_select_cb(SSL_CTX *ctx,
                                SSL_CTX_alpn_select_cb_func cb,
                                void *arg)
{
    ctx->ext.alpn_select_cb = cb;
    ctx->ext.alpn_select_cb_arg = arg;
}

int s_server_main(int argc, char *argv[])
{
    SSL_CTX *ctx = NULL;
    tlsextalpnctx alpn_ctx = { NULL, 0 };
    (void)argc; (void)argv;
    if (alpn_ctx.data)
        SSL_CTX_set_alpn_select_cb(ctx, alpn_cb, &alpn_ctx);
    return 0;
}
