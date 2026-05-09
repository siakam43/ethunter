/* CG-Bench fixture: fnptr-callback/example_12 */
/* fnptr: hash, targets: Curl_md5it, Curl_sha256it */

static CURLcode auth_create_digest_http_message(
                  struct Curl_easy *data,
                  const char *userp,
                  const char *passwdp,
                  const unsigned char *request,
                  const unsigned char *uripath,
                  struct digestdata *digest,
                  char **outptr, size_t *outlen,
                  void (*convert_to_ascii)(unsigned char *, unsigned char *),
                  CURLcode (*hash)(unsigned char *, const unsigned char *,
                                   const size_t))
{
  if(!hashthis)
    return CURLE_OUT_OF_MEMORY;

  hash(hashbuf, (unsigned char *) hashthis, strlen(hashthis));
  free(hashthis);
}

CURLcode Curl_auth_create_digest_http_message(struct Curl_easy *data,
                                              const char *userp,
                                              const char *passwdp,
                                              const unsigned char *request,
                                              const unsigned char *uripath,
                                              struct digestdata *digest,
                                              char **outptr, size_t *outlen)
{
  if(digest->algo <= ALGO_MD5SESS)
    return auth_create_digest_http_message(data, userp, passwdp,
                                           request, uripath, digest,
                                           outptr, outlen,
                                           auth_digest_md5_to_ascii,
                                           Curl_md5it);
  DEBUGASSERT(digest->algo <= ALGO_SHA512_256SESS);
  return auth_create_digest_http_message(data, userp, passwdp,
                                         request, uripath, digest,
                                         outptr, outlen,
                                         auth_digest_sha256_to_ascii,
                                         Curl_sha256it);
}


/* Stub implementation for Curl_md5it */
void Curl_md5it(void) {}



/* Stub implementation for Curl_sha256it */
void Curl_sha256it(void) {}
