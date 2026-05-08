/* CG-Bench fixture: fnptr-callback/example_1 */
/* fnptr: convert_to_ascii, targets: auth_digest_md5_to_ascii, auth_digest_sha256_to_ascii */

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
    if(digest->userhash) {
    hashthis = aprintf("%s:%s", userp, digest->realm ? digest->realm : "");
    if(!hashthis)
        return CURLE_OUT_OF_MEMORY;
    
    hash(hashbuf, (unsigned char *) hashthis, strlen(hashthis));
    free(hashthis);
    convert_to_ascii(hashbuf, (unsigned char *)userh);
    }
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


/* Wrapper: calls through convert_to_ascii */
void convert_to_ascii_caller(void) {
    convert_to_ascii();
}



/* Stub implementation for auth_digest_md5_to_ascii */
void auth_digest_md5_to_ascii(void) {}



/* Stub implementation for auth_digest_sha256_to_ascii */
void auth_digest_sha256_to_ascii(void) {}
