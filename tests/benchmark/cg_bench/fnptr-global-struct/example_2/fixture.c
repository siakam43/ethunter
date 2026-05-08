/* CG-Bench fixture: fnptr-global-struct/example_2 */
/* fnptr: Curl_ssl->sha256sum, targets: ossl_sha256sum */

CURLcode Curl_pin_peer_pubkey(struct Curl_easy *data,
                              const char *pinnedpubkey,
                              const unsigned char *pubkey, size_t pubkeylen)
{
...
/* compute sha256sum of public key */
   sha256sumdigest = malloc(CURL_SHA256_DIGEST_LENGTH);
   if(!sha256sumdigest)
     return CURLE_OUT_OF_MEMORY;
   encode = Curl_ssl->sha256sum(pubkey, pubkeylen,
                                sha256sumdigest, CURL_SHA256_DIGEST_LENGTH);
...
}

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

const struct Curl_ssl Curl_ssl_openssl = {
  { CURLSSLBACKEND_OPENSSL, "openssl" }, /* info */
...
  ossl_get_internals,       /* get_internals */
  ossl_close,               /* close_one */
  ossl_close_all,           /* close_all */
  ossl_session_free,        /* session_free */
  ossl_set_engine,          /* set_engine */
  ossl_set_engine_default,  /* set_engine_default */
  ossl_engines_list,        /* engines_list */
  Curl_none_false_start,    /* false_start */
#if (OPENSSL_VERSION_NUMBER >= 0x0090800fL) && !defined(OPENSSL_NO_SHA256)
  ossl_sha256sum,           /* sha256sum */
#else
  NULL,                     /* sha256sum */
#endif
  NULL,                     /* use of data in this connection */
...
};


/* Stub implementation for ossl_sha256sum */
void ossl_sha256sum(void) {}
