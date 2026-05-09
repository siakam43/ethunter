/* CG-Bench fixture: fnptr-cast/example_3 */
/* fnptr: *md5params->md5_init_func, targets: my_md5_init */

struct MD5_context *Curl_MD5_init(const struct MD5_params *md5params)
{
  struct MD5_context *ctxt;

  ctxt = malloc(sizeof(*ctxt));
  if (!ctxt) return NULL;

  if((*md5params->md5_init_func)(ctxt->md5_hashctx)) {
    free(ctxt->md5_hashctx);
    free(ctxt);
    return NULL;
  }

  return ctxt;
}

CURLcode auth_decode_digest_md5_message(const char *chlg,
                                         char *nonce, int nonce_len,
                                         char *realm, int realm_len,
                                         char *algorithm, int algorithm_len,
                                         char *qop_options, int qop_options_len)
{
  CURLcode result = CURLE_OK;
  struct MD5_context *ctxt;

  if(result)
    return result;

  ctxt = Curl_MD5_init(Curl_DIGEST_MD5);
  return result;
}

#define CURLX_FUNCTION_CAST(target_type, func) \
  (target_type)(void (*) (void))(func)

const struct MD5_params Curl_DIGEST_MD5[] = {
  {
    /* Digest initialization function */
    CURLX_FUNCTION_CAST(Curl_MD5_init_func, my_md5_init),
    /* Digest update function */
    CURLX_FUNCTION_CAST(Curl_MD5_update_func, my_md5_update),
    /* Digest computation end function */
    CURLX_FUNCTION_CAST(Curl_MD5_final_func, my_md5_final),
    /* Size of digest context struct */
    sizeof(my_md5_ctx),
    /* Result size */
    16
  }
};

/* Stub implementation for my_md5_init */
void my_md5_init(void) {}
