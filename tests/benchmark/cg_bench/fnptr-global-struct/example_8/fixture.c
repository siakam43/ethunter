/* CG-Bench fixture: fnptr-global-struct/example_8 */
/* fnptr: Curl_ssl->send_plain, targets: multissl_send_plain */

static ssize_t multissl_send_plain(struct Curl_cfilter *cf,
                                   struct Curl_easy *data,
                                   const void *mem, size_t len,
                                   CURLcode *code)
{
  if(multissl_setup(NULL))
    return CURLE_FAILED_INIT;
  return Curl_ssl->send_plain(cf, data, mem, len, code);
}

static const struct Curl_ssl Curl_ssl_multi = {
  { CURLSSLBACKEND_NONE, "multi" },  /* info */
  0, /* supports nothing */
  (size_t)-1, /* something insanely large to be on the safe side */

  multissl_init,                     /* init */
  Curl_none_cleanup,                 /* cleanup */
  multissl_version,                  /* version */
  Curl_none_check_cxn,               /* check_cxn */
  Curl_none_shutdown,                /* shutdown */
  Curl_none_data_pending,            /* data_pending */
  Curl_none_random,                  /* random */
  Curl_none_cert_status_request,     /* cert_status_request */
  multissl_connect,                  /* connect */
  multissl_connect_nonblocking,      /* connect_nonblocking */
  multissl_adjust_pollset,          /* adjust_pollset */
  multissl_get_internals,            /* get_internals */
  multissl_close,                    /* close_one */
  Curl_none_close_all,               /* close_all */
  Curl_none_session_free,            /* session_free */
  Curl_none_set_engine,              /* set_engine */
  Curl_none_set_engine_default,      /* set_engine_default */
  Curl_none_engines_list,            /* engines_list */
  Curl_none_false_start,             /* false_start */
  NULL,                              /* sha256sum */
  NULL,                              /* associate_connection */
  NULL,                              /* disassociate_connection */
  NULL,                              /* free_multi_ssl_backend_data */
  multissl_recv_plain,               /* recv decrypted data */
  multissl_send_plain,               /* send data to encrypt */
};

const struct Curl_ssl *Curl_ssl =
#if defined(CURL_WITH_MULTI_SSL)
  &Curl_ssl_multi;
#elif defined(USE_WOLFSSL)
  &Curl_ssl_wolfssl;
#elif defined(USE_SECTRANSP)
  &Curl_ssl_sectransp;
#elif defined(USE_GNUTLS)
  &Curl_ssl_gnutls;
#elif defined(USE_MBEDTLS)
  &Curl_ssl_mbedtls;
#elif defined(USE_RUSTLS)
  &Curl_ssl_rustls;
#elif defined(USE_OPENSSL)
  &Curl_ssl_openssl;
#elif defined(USE_SCHANNEL)
  &Curl_ssl_schannel;
#elif defined(USE_BEARSSL)
  &Curl_ssl_bearssl;
#else
#error "Missing struct Curl_ssl for selected SSL backend"
#endif

struct Curl_ssl {
  /*
   * This *must* be the first entry to allow returning the list of available
   * backends in curl_global_sslset().
   */
  curl_ssl_backend info;
  unsigned int supports; /* bitfield, see above */
  size_t sizeof_ssl_backend_data;

  int (*init)(void);
  void (*cleanup)(void);

  size_t (*version)(char *buffer, size_t size);
  int (*check_cxn)(struct Curl_cfilter *cf, struct Curl_easy *data);
  int (*shut_down)(struct Curl_cfilter *cf,
                   struct Curl_easy *data);
  bool (*data_pending)(struct Curl_cfilter *cf,
                       const struct Curl_easy *data);

  /* return 0 if a find random is filled in */
  CURLcode (*random)(struct Curl_easy *data, unsigned char *entropy,
                     size_t length);
  bool (*cert_status_request)(void);

  CURLcode (*connect_blocking)(struct Curl_cfilter *cf,
                               struct Curl_easy *data);
  CURLcode (*connect_nonblocking)(struct Curl_cfilter *cf,
                                  struct Curl_easy *data,
                                  bool *done);

  /* During handshake, adjust the pollset to include the socket
   * for POLLOUT or POLLIN as needed.
   * Mandatory. */
  void (*adjust_pollset)(struct Curl_cfilter *cf, struct Curl_easy *data,
                          struct easy_pollset *ps);
  void *(*get_internals)(struct ssl_connect_data *connssl, CURLINFO info);
  void (*close)(struct Curl_cfilter *cf, struct Curl_easy *data);
  void (*close_all)(struct Curl_easy *data);
  void (*session_free)(void *ptr);

  CURLcode (*set_engine)(struct Curl_easy *data, const char *engine);
  CURLcode (*set_engine_default)(struct Curl_easy *data);
  struct curl_slist *(*engines_list)(struct Curl_easy *data);

  bool (*false_start)(void);
  CURLcode (*sha256sum)(const unsigned char *input, size_t inputlen,
                    unsigned char *sha256sum, size_t sha256sumlen);

  bool (*attach_data)(struct Curl_cfilter *cf, struct Curl_easy *data);
  void (*detach_data)(struct Curl_cfilter *cf, struct Curl_easy *data);

  void (*free_multi_ssl_backend_data)(struct multi_ssl_backend_data *mbackend);

  ssize_t (*recv_plain)(struct Curl_cfilter *cf, struct Curl_easy *data,
                        char *buf, size_t len, CURLcode *code);
  ssize_t (*send_plain)(struct Curl_cfilter *cf, struct Curl_easy *data,
                        const void *mem, size_t len, CURLcode *code);

};