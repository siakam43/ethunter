/* ET-Bench fixture: fnptr-library/example_8 */
/* fnptr: cf->cft->get_host, targets: cf_socket_get_host */
/* Pattern: library filter chain with type struct containing function pointers */

#include <stdio.h>
#include <stdlib.h>

#define DEBUGASSERT(x)
#define FALSE 0
#define CURL_LOG_LVL_NONE 0
#define CF_TYPE_IP_CONNECT 1

struct Curl_easy {
    struct connectdata *conn;
};

struct connectdata {
    struct Curl_cfilter *cfilter[8];
    int transport;
};

typedef struct Curl_cfilter {
    struct Curl_cftype *cft;
    struct connectdata *conn;
    struct Curl_cfilter *next;
    int sockindex;
} Curl_cfilter;

/* Forward declarations for function pointer types */
typedef void Curl_cft_destroy_this(Curl_cfilter *cf);
typedef void Curl_cft_connect(Curl_cfilter *cf, struct Curl_easy *data);
typedef void Curl_cft_close(Curl_cfilter *cf);
typedef void Curl_cft_get_host(Curl_cfilter *cf, struct Curl_easy *data,
                               const char **phost, const char **pdisplay_host, int *pport);
typedef void Curl_cft_adjust_pollset(Curl_cfilter *cf, void *transfer);
typedef int Curl_cft_data_pending(Curl_cfilter *cf);
typedef int Curl_cft_send(Curl_cfilter *cf, void *data, void *buf, int len);
typedef int Curl_cft_recv(Curl_cfilter *cf, void *data, void *buf, int len);
typedef void Curl_cft_cntrl(Curl_cfilter *cf, int event);
typedef int Curl_cft_conn_is_alive(Curl_cfilter *cf);
typedef int Curl_cft_conn_keep_alive(Curl_cfilter *cf);
typedef void Curl_cft_query(Curl_cfilter *cf, struct Curl_easy *data, int sockindex);

struct Curl_cftype {
    const char *name;
    int flags;
    int log_level;
    Curl_cft_destroy_this *destroy;
    Curl_cft_connect *do_connect;
    Curl_cft_close *do_close;
    Curl_cft_get_host *get_host;
    Curl_cft_adjust_pollset *adjust_pollset;
    Curl_cft_data_pending *has_data_pending;
    Curl_cft_send *do_send;
    Curl_cft_recv *do_recv;
    Curl_cft_cntrl *cntrl;
    Curl_cft_conn_is_alive *is_alive;
    Curl_cft_conn_keep_alive *keep_alive;
    Curl_cft_query *query;
};

static void cf_socket_destroy(Curl_cfilter *cf) { (void)cf; }
static void cf_tcp_connect(Curl_cfilter *cf, struct Curl_easy *data) { (void)cf; (void)data; }
static void cf_socket_close(Curl_cfilter *cf) { (void)cf; }
static void cf_socket_adjust_pollset(Curl_cfilter *cf, void *transfer) { (void)cf; (void)transfer; }
static int cf_socket_data_pending(Curl_cfilter *cf) { (void)cf; return 0; }
static int cf_socket_send(Curl_cfilter *cf, void *data, void *buf, int len) { (void)cf; (void)data; (void)buf; (void)len; return 0; }
static int cf_socket_recv(Curl_cfilter *cf, void *data, void *buf, int len) { (void)cf; (void)data; (void)buf; (void)len; return 0; }
static void cf_socket_cntrl(Curl_cfilter *cf, int event) { (void)cf; (void)event; }
static int cf_socket_conn_is_alive(Curl_cfilter *cf) { (void)cf; return 1; }
static int Curl_cf_def_conn_keep_alive(Curl_cfilter *cf) { (void)cf; return 1; }
static void Curl_cf_query(Curl_cfilter *cf, struct Curl_easy *data, int sockindex) { (void)cf; (void)data; (void)sockindex; }

/* Target: set in Curl_cft_tcp */
static void cf_socket_get_host(Curl_cfilter *cf, struct Curl_easy *data,
                               const char **phost, const char **pdisplay_host, int *pport) {
    (void)cf; (void)data; (void)phost; (void)pdisplay_host; (void)pport;
}

struct Curl_cftype Curl_cft_tcp = {
    "TCP",
    CF_TYPE_IP_CONNECT,
    CURL_LOG_LVL_NONE,
    cf_socket_destroy,
    cf_tcp_connect,
    cf_socket_close,
    cf_socket_get_host,
    cf_socket_adjust_pollset,
    cf_socket_data_pending,
    cf_socket_send,
    cf_socket_recv,
    cf_socket_cntrl,
    cf_socket_conn_is_alive,
    Curl_cf_def_conn_keep_alive,
    Curl_cf_query,
};

void Curl_conn_get_host(struct Curl_easy *data, int sockindex,
                        const char **phost, const char **pdisplay_host,
                        int *pport)
{
    struct Curl_cfilter *cf;

    DEBUGASSERT(data->conn);
    cf = data->conn->cfilter[sockindex];
    if (cf) {
        cf->cft->get_host(cf, data, phost, pdisplay_host, pport);
    }
}

void Curl_conn_cf_add(struct Curl_easy *data,
                      struct connectdata *conn,
                      int index,
                      struct Curl_cfilter *cf)
{
    (void)data;
    DEBUGASSERT(conn);
    DEBUGASSERT(!cf->conn);
    DEBUGASSERT(!cf->next);

    cf->next = conn->cfilter[index];
    cf->conn = conn;
    cf->sockindex = index;
    conn->cfilter[index] = cf;
}
