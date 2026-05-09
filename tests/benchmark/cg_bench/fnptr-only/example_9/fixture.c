/* CG-Bench fixture: fnptr-only/example_9 */
/* fnptr: Curl_cfree, targets: free */

CURLcode Curl_output_digest(struct Curl_easy *data,
                            bool proxy,
                            const unsigned char *request,
                            const unsigned char *uripath)
{
  CURLcode result;
  ...

  Curl_safefree(*allocuserpwd);

  /* not set means empty */
  if(!userp)
    userp = "";

  if(!passwdp)
    passwdp = "";

  ...
  
  return CURLE_OK;
}

#define Curl_safefree(ptr) \
  do { Curl_cfree((ptr)); (ptr) = NULL;} while(0)


curl_free_callback Curl_cfree = (curl_free_callback)free;





/* Stub implementation for free */
void free(ptr) {}
