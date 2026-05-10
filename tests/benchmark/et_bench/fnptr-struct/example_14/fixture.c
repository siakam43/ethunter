/* ET-Bench fixture: fnptr-struct/example_14 */
/* Scenario: OpenSSL SSL handshake function pointer dispatch.
   fnptr: s->handshake_func
   targets: ossl_statem_accept, ossl_statem_connect
   caller: ssl3_write_bytes */

#include <stddef.h>
#include <stdlib.h>

typedef struct SSL SSL;
typedef struct SSL_CTX SSL_CTX;
typedef struct BUF_MEM BUF_MEM;

typedef int (ossl_statem_fn)(SSL *s);

#define SSL_IS_TLS13(s) ((s)->version == 0x0304)
#define SSL_IS_DTLS(s) (0)
#define SSL_IS_FIRST_HANDSHAKE(s) (1)

#define SSL_CB_HANDSHAKE_DONE 0x20

#define SSL_PHA_REQUESTED 1
#define SSL_PHA_EXT_SENT 2
#define SSL_SESS_CACHE_SERVER 0x0001
#define SSL_SESS_CACHE_CLIENT 0x0002

#define SSL_F_TLS_FINISH_HANDSHAKE 100
#define ERR_R_INTERNAL_ERROR 1

typedef struct ssl_statem {
    int cleanuphand;
} ssl_statem;

typedef struct ssl3_data {
    int handshake_read_seq;
    int handshake_write_seq;
    int next_handshake_write_seq;
} ssl3_data;

typedef struct session_st {
    int version;
} SSL_SESSION;

struct SSL_CTX {
    int session_cache_mode;
    struct {
        int sess_accept_good;
        int sess_hit;
        int sess_connect_good;
    } stats;
    struct {
        int session_cache_mode;
    } scache;
};

struct SSL {
    int version;
    int early_data_state;
    ossl_statem_fn *handshake_func;
    ssl_statem statem;
    ssl3_data d1;
    int renegotiate;
    int new_session;
    int hit;
    int post_handshake_auth;
    int server;
    int init_num;
    BUF_MEM *init_buf;
    SSL_CTX *ctx;
    SSL_CTX *session_ctx;
    SSL_SESSION *session;
    void (*info_callback)(const SSL *ssl, int type, int val);
};

struct BUF_MEM {
    char *data;
    size_t length;
};

#define SSL_EARLY_DATA_UNAUTH_WRITING 0
#define SSL_AD_INTERNAL_ERROR 80

static int ossl_statem_get_in_handshake(SSL *s) { (void)s; return 0; }
static void ossl_statem_set_in_init(SSL *s, int in_init) { (void)s; (void)in_init; }
static int SSL_in_init(SSL *s) { (void)s; return 1; }
static void ssl3_cleanup_key_block(SSL *s) { (void)s; }
static void BUF_MEM_free(BUF_MEM *b) { (void)b; }
static int ssl_free_wbio_buffer(SSL *s) { (void)s; return 1; }
static void ssl_update_cache(SSL *s, int mode) { (void)s; (void)mode; }
static void SSL_CTX_remove_session(SSL_CTX *ctx, SSL_SESSION *s) { (void)ctx; (void)s; }
static void SSLfatal(SSL *s, int alert, int func, int reason) { (void)s; (void)alert; (void)func; (void)reason; }

/* Targets: handshake state machine functions */
static int ossl_statem_accept(SSL *s)
{
    (void)s;
    return 1;
}

static int ossl_statem_connect(SSL *s)
{
    (void)s;
    return 1;
}

/* Caller: invokes s->handshake_func through the struct */
int ssl3_write_bytes(SSL *s, int type, const void *buf_, size_t len,
                     size_t *written)
{
    int i;
    (void)type; (void)buf_; (void)len; (void)written;

    if (SSL_in_init(s) && !ossl_statem_get_in_handshake(s)
            && s->early_data_state != SSL_EARLY_DATA_UNAUTH_WRITING) {
        i = s->handshake_func(s);
        if (i < 0)
            return i;
        if (i == 0) {
            return -1;
        }
    }
    return (int)len;
}

int tls_finish_handshake(SSL *s, int wst, int clearbufs, int stop)
{
    void (*cb)(const SSL *ssl, int type, int val) = NULL;
    int cleanuphand = s->statem.cleanuphand;
    (void)wst;

    if (clearbufs) {
        if (!SSL_IS_DTLS(s)) {
            BUF_MEM_free(s->init_buf);
            s->init_buf = NULL;
        }

        if (!ssl_free_wbio_buffer(s)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_TLS_FINISH_HANDSHAKE,
                     ERR_R_INTERNAL_ERROR);
            return -1;
        }
        s->init_num = 0;
    }

    if (SSL_IS_TLS13(s) && !s->server
            && s->post_handshake_auth == SSL_PHA_REQUESTED)
        s->post_handshake_auth = SSL_PHA_EXT_SENT;

    if (cleanuphand) {
        s->renegotiate = 0;
        s->new_session = 0;
        s->statem.cleanuphand = 0;

        ssl3_cleanup_key_block(s);

        if (s->server) {
            if (!SSL_IS_TLS13(s))
                ssl_update_cache(s, SSL_SESS_CACHE_SERVER);
            s->handshake_func = ossl_statem_accept;
        } else {
            if (SSL_IS_TLS13(s)) {
                if ((s->session_ctx->session_cache_mode
                     & SSL_SESS_CACHE_CLIENT) != 0)
                    SSL_CTX_remove_session(s->session_ctx, s->session);
            } else {
                ssl_update_cache(s, SSL_SESS_CACHE_CLIENT);
            }
            if (s->hit)
                s->session_ctx->stats.sess_hit++;

            s->handshake_func = ossl_statem_connect;
            s->session_ctx->stats.sess_connect_good++;
        }

        if (SSL_IS_DTLS(s)) {
            s->d1.handshake_read_seq = 0;
            s->d1.handshake_write_seq = 0;
            s->d1.next_handshake_write_seq = 0;
        }
    }

    if (s->info_callback != NULL)
        cb = s->info_callback;
    else if (s->ctx->info_callback != NULL)
        cb = s->ctx->info_callback;

    ossl_statem_set_in_init(s, 0);

    if (cb != NULL) {
        if (cleanuphand
                || !SSL_IS_TLS13(s)
                || SSL_IS_FIRST_HANDSHAKE(s))
            cb(s, SSL_CB_HANDSHAKE_DONE, 1);
    }

    if (!stop) {
        ossl_statem_set_in_init(s, 1);
        return 1; /* WORK_FINISHED_CONTINUE */
    }

    return 0; /* WORK_FINISHED_STOP */
}
