/* CG-Bench fixture: fnptr-only/example_2 */
/* fnptr: Curl_ccalloc, targets: calloc */

static const char *dsthost Curl_new(
                                      enum alpnid srcalpnid,
                                      enum alpnid dstalpnid,
                                      unsigned int srcport,
                                      unsigned int dstport)
{
  struct altsvc *as = zcalloc(1, sizeof(struct altsvc));
  size_t hlen;
  size_t dlen;
  if(!as)
    return NULL;
}

#define zcalloc(nbelem,size) Curl_ccalloc(nbelem, size)

curl_calloc_callback Curl_ccalloc = (curl_calloc_callback)calloc;

static CURLcode global_init(long flags, bool memoryfuncs)
{
  if(initialized++)
    return CURLE_OK;

  if(memoryfuncs) {
    /* Setup the default memory functions here (again) */
    Curl_cmalloc = (curl_malloc_callback)malloc;
    Curl_cfree = (curl_free_callback)free;
    Curl_crealloc = (curl_realloc_callback)realloc;
    Curl_cstrdup = (curl_strdup_callback)system_strdup;
    Curl_ccalloc = (curl_calloc_callback)calloc;
    ...
  }
}





/* Stub implementation for calloc */
void calloc(nbelem,size) {}
