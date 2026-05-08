/* CG-Bench fixture: fnptr-library/example_8 */
/* fnptr: cf->cft->get_host, targets: cf_socket_get_host */

void Curl_conn_get_host(struct Curl_easy *data, int sockindex,
                        const char **phost, const char **pdisplay_host,
                        int *pport)
{
  struct Curl_cfilter *cf;

  DEBUGASSERT(data->conn);
  cf = data->conn->cfilter[sockindex];
  if(cf) {
    cf->cft->get_host(cf, data, phost, pdisplay_host, pport);
  }
  ...
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
  cf_socket_query,
};

struct Curl_cftype {
  const char *name;                       /* name of the filter type */
  int flags;                              /* flags of filter type */
  int log_level;                          /* log level for such filters */
  Curl_cft_destroy_this *destroy;         /* destroy resources of this cf */
  Curl_cft_connect *do_connect;           /* establish connection */
  Curl_cft_close *do_close;               /* close conn */
  Curl_cft_get_host *get_host;            /* host filter talks to */
  Curl_cft_adjust_pollset *adjust_pollset; /* adjust transfer poll set */
  Curl_cft_data_pending *has_data_pending;/* conn has data pending */
  Curl_cft_send *do_send;                 /* send data */
  Curl_cft_recv *do_recv;                 /* receive data */
  Curl_cft_cntrl *cntrl;                  /* events/control */
  Curl_cft_conn_is_alive *is_alive;       /* FALSE if conn is dead, Jim! */
  Curl_cft_conn_keep_alive *keep_alive;   /* try to keep it alive */
  Curl_cft_query *query;                  /* query filter chain */
};

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
  CURL_TRC_CF(data, cf, "added");
}

CURLcode Curl_conn_tcp_listen_set(struct Curl_easy *data,
                                  struct connectdata *conn,
                                  int sockindex, curl_socket_t *s)
{
  CURLcode result;
  struct Curl_cfilter *cf = NULL;
  struct cf_socket_ctx *ctx = NULL;

  /* replace any existing */
  Curl_conn_cf_discard_all(data, conn, sockindex);
  DEBUGASSERT(conn->sock[sockindex] == CURL_SOCKET_BAD);

  ctx = calloc(1, sizeof(*ctx));
  if(!ctx) {
    result = CURLE_OUT_OF_MEMORY;
    goto out;
  }
  ctx->transport = conn->transport;
  ctx->sock = *s;
  ctx->accepted = FALSE;
  result = Curl_cf_create(&cf, &Curl_cft_tcp_accept, ctx);
  if(result)
    goto out;
  Curl_conn_cf_add(data, conn, sockindex, cf);
  ...
}


/* Stub implementation for cf_socket_get_host */
void cf_socket_get_host(void) {}
