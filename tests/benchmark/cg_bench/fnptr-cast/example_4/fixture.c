/* CG-Bench fixture: fnptr-cast/example_4 */
/* fnptr: *context->md5_hash->md5_update_func, targets: my_md5_update */

CURLcode Curl_MD5_update(struct MD5_context *context,
                         const unsigned char *data,
                         unsigned int len)
{
  (*context->md5_hash->md5_update_func)(context->md5_hashctx, data, len);

  return CURLE_OK;
}

static CURLcode pop3_perform_apop(struct Curl_easy *data,
                                  struct connectdata *conn)
{
  CURLcode result = CURLE_OK;
  struct MD5_context *ctxt;
  /* Create the digest */
  ctxt = Curl_MD5_init(Curl_DIGEST_MD5);
  if(!ctxt)
     return CURLE_OUT_OF_MEMORY;
  Curl_MD5_update(ctxt, (const unsigned char *) pop3c->apoptimestamp,
                  curlx_uztoui(strlen(pop3c->apoptimestamp)));
  return result;
}

struct MD5_context *Curl_MD5_init(const struct MD5_params *md5params)
{
  struct MD5_context *ctxt;

  /* Create MD5 context */
  ctxt = malloc(sizeof(*ctxt));
  ctxt->md5_hash = md5params;
  return ctxt;
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

/* Stub implementation for my_md5_update */
void my_md5_update(void) {}
