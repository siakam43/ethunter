/* ET-Bench fixture: fnptr-struct/example_11 */
/* Scenario: OpenSSL SSL method vtable — put_cipher_by_char dispatch.
   fnptr: s->method->put_cipher_by_char
   target: ssl3_put_cipher_by_char
   caller: ssl_cipher_list_to_bytes */

#include <stddef.h>
#include <stdint.h>

#define SSL3_CK_CIPHERSUITE_FLAG 0x03000000

typedef struct WPACKET {
    unsigned char *buf;
    size_t len;
    size_t max_len;
} WPACKET;

typedef struct ssl3_enc_method {
    int enc_level;
} ssl3_enc_method;

typedef struct SSL_CIPHER {
    unsigned int id;
} SSL_CIPHER;

typedef struct SSL SSL;
typedef struct SSL_CTX SSL_CTX;
typedef struct SSL_METHOD SSL_METHOD;
typedef struct stack_st_SSL_CIPHER STACK_OF_SSL_CIPHER;

struct ssl_method_st {
    int version;
    unsigned flags;
    unsigned long mask;
    int (*ssl_new)(SSL *s);
    int (*ssl_clear)(SSL *s);
    void (*ssl_free)(SSL *s);
    int (*ssl_accept)(SSL *s);
    int (*ssl_connect)(SSL *s);
    int (*ssl_read)(SSL *s, void *buf, size_t len, size_t *readbytes);
    int (*ssl_peek)(SSL *s, void *buf, size_t len, size_t *readbytes);
    int (*ssl_write)(SSL *s, const void *buf, size_t len, size_t *written);
    int (*ssl_shutdown)(SSL *s);
    int (*ssl_renegotiate)(SSL *s);
    int (*ssl_renegotiate_check)(SSL *s, int);
    int (*ssl_read_bytes)(SSL *s, int type, int *recvd_type,
                          unsigned char *buf, size_t len, int peek,
                          size_t *readbytes);
    int (*ssl_write_bytes)(SSL *s, int type, const void *buf_, size_t len,
                           size_t *written);
    int (*ssl_dispatch_alert)(SSL *s);
    long (*ssl_ctrl)(SSL *s, int cmd, long larg, void *parg);
    long (*ssl_ctx_ctrl)(SSL_CTX *ctx, int cmd, long larg, void *parg);
    const SSL_CIPHER *(*get_cipher_by_char)(const unsigned char *ptr);
    int (*put_cipher_by_char)(const SSL_CIPHER *cipher, WPACKET *pkt,
                              size_t *len);
    size_t (*ssl_pending)(const SSL *s);
    int (*num_ciphers)(void);
    const SSL_CIPHER *(*get_cipher)(unsigned ncipher);
    long (*get_timeout)(void);
    const struct ssl3_enc_method *ssl3_enc;
    int (*ssl_version)(void);
    long (*ssl_callback_ctrl)(SSL *s, int cb_id, void (*fp)(void));
    long (*ssl_ctx_callback_ctrl)(SSL_CTX *s, int cb_id, void (*fp)(void));
};

struct SSL {
    SSL_METHOD *method;
    int version;
};

static int WPACKET_put_bytes_u16(WPACKET *pkt, uint16_t val) {
    if (pkt->len + 2 > pkt->max_len)
        return 0;
    pkt->buf[pkt->len++] = (val >> 8) & 0xff;
    pkt->buf[pkt->len++] = val & 0xff;
    return 1;
}

/* Target: ssl3 put cipher by char */
int ssl3_put_cipher_by_char(const SSL_CIPHER *c, WPACKET *pkt, size_t *len)
{
    if ((c->id & 0xff000000) != SSL3_CK_CIPHERSUITE_FLAG) {
        *len = 0;
        return 1;
    }

    if (!WPACKET_put_bytes_u16(pkt, c->id & 0xffff))
        return 0;

    *len = 2;
    return 1;
}

static const SSL_METHOD ssl3_method = {
    .ssl_new = tls1_new,
    .ssl_clear = tls1_clear,
    .ssl_free = tls1_free,
    .ssl_accept = ssl_undefined_void_function,
    .ssl_connect = ssl_undefined_void_function,
    .get_cipher_by_char = ssl3_get_cipher_by_char,
    .put_cipher_by_char = ssl3_put_cipher_by_char,
    .num_ciphers = ssl3_num_ciphers,
    .get_cipher = ssl3_get_cipher,
    .get_timeout = tls1_default_timeout,
    .ssl3_enc = &ssl3_enc_data,
    .ssl_version = ssl_undefined_void_function,
    .ssl_callback_ctrl = ssl3_callback_ctrl,
    .ssl_ctx_callback_ctrl = ssl3_ctx_callback_ctrl,
};

/* Caller: invokes s->method->put_cipher_by_char through the struct */
int ssl_cipher_list_to_bytes(SSL *s, STACK_OF_SSL_CIPHER *sk, WPACKET *pkt)
{
    SSL_CIPHER *c = NULL;
    size_t len;
    (void)sk;
    s->method = &ssl3_method;

    if (!s->method->put_cipher_by_char(c, pkt, &len)) {
        return 0;
    }
    return (int)len;
}

static int tls1_new(SSL *s) { (void)s; return 1; }
static int tls1_clear(SSL *s) { (void)s; return 1; }
static void tls1_free(SSL *s) { (void)s; }
static int tls1_default_timeout(void) { return 7200; }

const SSL_CIPHER *ssl3_get_cipher_by_char(const unsigned char *ptr) {
    (void)ptr; return NULL;
}

const SSL_CIPHER *ssl3_get_cipher(unsigned ncipher) { (void)ncipher; return NULL; }
int ssl3_num_ciphers(void) { return 0; }
int ssl_undefined_void_function(void) { return 0; }
long ssl3_callback_ctrl(SSL *s, int cb_id, void (*fp)(void)) { (void)s; (void)cb_id; (void)fp; return 0; }
long ssl3_ctx_callback_ctrl(SSL_CTX *ctx, int cb_id, void (*fp)(void)) { (void)ctx; (void)cb_id; (void)fp; return 0; }

static const ssl3_enc_method ssl3_enc_data = {1};
