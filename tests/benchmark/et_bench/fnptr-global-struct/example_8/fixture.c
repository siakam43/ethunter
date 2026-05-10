/* et_bench fixture: fnptr-global-struct/example_8 */
/* fnptr: Curl_ssl->send_plain, targets: multissl_send_plain */

#include <stddef.h>
#include <stdint.h>

typedef int CURLcode;
#define CURLE_FAILED_INIT 1

struct Curl_cfilter { int id; };
struct Curl_easy { int flags; };
struct ssl_connect_data { int state; };

typedef struct Curl_ssl Curl_ssl_t;

struct Curl_ssl {
    int backend_id;
    const char *name;
    size_t sizeof_ssl_backend_data;
    int (*init)(void);
    void (*cleanup)(void);
    size_t (*version)(char *buffer, size_t size);
    int (*check_cxn)(struct Curl_cfilter *cf, struct Curl_easy *data);
    int (*shut_down)(struct Curl_cfilter *cf, struct Curl_easy *data);
    int (*data_pending)(struct Curl_cfilter *cf, const struct Curl_easy *data);
    int (*random)(struct Curl_easy *data, unsigned char *entropy, size_t len);
    int (*cert_status_request)(void);
    CURLcode (*connect_blocking)(struct Curl_cfilter *cf, struct Curl_easy *data);
    CURLcode (*connect_nonblocking)(struct Curl_cfilter *cf, struct Curl_easy *data, int *done);
    void (*adjust_pollset)(struct Curl_cfilter *cf, struct Curl_easy *data, void *ps);
    void *(*get_internals)(struct ssl_connect_data *connssl, int info);
    void (*close)(struct Curl_cfilter *cf, struct Curl_easy *data);
    void (*close_all)(struct Curl_easy *data);
    void (*session_free)(void *ptr);
    CURLcode (*set_engine)(struct Curl_easy *data, const char *engine);
    CURLcode (*set_engine_default)(struct Curl_easy *data);
    void *(*engines_list)(struct Curl_easy *data);
    int (*false_start)(void);
    CURLcode (*sha256sum)(const unsigned char *input, size_t inputlen,
                          unsigned char *sha256sum, size_t sha256sumlen);
    int (*attach_data)(struct Curl_cfilter *cf, struct Curl_easy *data);
    void (*detach_data)(struct Curl_cfilter *cf, struct Curl_easy *data);
    void (*free_multi_ssl_backend_data)(void *mbackend);
    ssize_t (*recv_plain)(struct Curl_cfilter *cf, struct Curl_easy *data,
                          char *buf, size_t len, CURLcode *code);
    ssize_t (*send_plain)(struct Curl_cfilter *cf, struct Curl_easy *data,
                          const void *mem, size_t len, CURLcode *code);
};

/* Multi-SSL implementations */
int multissl_setup(int **backend) { return 0; }
int multissl_init(void) { return 0; }
size_t multissl_version(char *buf, size_t sz) { return 0; }
CURLcode multissl_connect(struct Curl_cfilter *cf, struct Curl_easy *data) { return 0; }
CURLcode multissl_connect_nonblocking(struct Curl_cfilter *cf, struct Curl_easy *data, int *done) { return 0; }
void multissl_adjust_pollset(struct Curl_cfilter *cf, struct Curl_easy *data, void *ps) {}
void *multissl_get_internals(struct ssl_connect_data *cs, int info) { return NULL; }
void multissl_close(struct Curl_cfilter *cf, struct Curl_easy *data) {}
ssize_t multissl_recv_plain(struct Curl_cfilter *cf, struct Curl_easy *data, char *buf, size_t len, CURLcode *code) { return 0; }
ssize_t multissl_send_plain(struct Curl_cfilter *cf, struct Curl_easy *data, const void *mem, size_t len, CURLcode *code)
{
    if (multissl_setup(NULL))
        return CURLE_FAILED_INIT;
    return Curl_ssl->send_plain(cf, data, mem, len, code);
}

/* Curl_none stubs */
static void Curl_none_cleanup(void) {}
static int Curl_none_check_cxn(struct Curl_cfilter *cf, struct Curl_easy *data) { return 0; }
static int Curl_none_shutdown(struct Curl_cfilter *cf, struct Curl_easy *data) { return 0; }
static int Curl_none_data_pending(struct Curl_cfilter *cf, const struct Curl_easy *data) { return 0; }
static int Curl_none_random(struct Curl_easy *data, unsigned char *e, size_t l) { return 0; }
static int Curl_none_cert_status_request(void) { return 0; }
static void Curl_none_close_all(struct Curl_easy *data) {}
static void Curl_none_session_free(void *ptr) {}
static CURLcode Curl_none_set_engine(struct Curl_easy *data, const char *e) { return 0; }
static CURLcode Curl_none_set_engine_default(struct Curl_easy *data) { return 0; }
static void *Curl_none_engines_list(struct Curl_easy *data) { return NULL; }
static int Curl_none_false_start(void) { return 0; }

const Curl_ssl_t Curl_ssl_multi = {
    .backend_id = 0,
    .name = "multi",
    .sizeof_ssl_backend_data = (size_t)-1,
    .init = multissl_init,
    .cleanup = Curl_none_cleanup,
    .version = multissl_version,
    .check_cxn = Curl_none_check_cxn,
    .shut_down = Curl_none_shutdown,
    .data_pending = Curl_none_data_pending,
    .random = Curl_none_random,
    .cert_status_request = Curl_none_cert_status_request,
    .connect_blocking = multissl_connect,
    .connect_nonblocking = multissl_connect_nonblocking,
    .adjust_pollset = multissl_adjust_pollset,
    .get_internals = multissl_get_internals,
    .close = multissl_close,
    .close_all = Curl_none_close_all,
    .session_free = Curl_none_session_free,
    .set_engine = Curl_none_set_engine,
    .set_engine_default = Curl_none_set_engine_default,
    .engines_list = Curl_none_engines_list,
    .false_start = Curl_none_false_start,
    .sha256sum = NULL,
    .attach_data = 0,
    .detach_data = 0,
    .free_multi_ssl_backend_data = NULL,
    .recv_plain = multissl_recv_plain,
    .send_plain = multissl_send_plain,
};

const Curl_ssl_t *Curl_ssl = &Curl_ssl_multi;
