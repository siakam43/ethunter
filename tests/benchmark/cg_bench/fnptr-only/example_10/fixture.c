/* CG-Bench fixture: fnptr-only/example_10 */
/* fnptr: Curl_cfree, targets: malloc */

CURLcode Curl_smtp_escape_eob(struct Curl_easy *data,
                              const ssize_t nread,
                              const ssize_t offset)
{
  ...

  /* Do we need to allocate a scratch buffer? */
  if(!scratch || data->set.crlf) {
    oldscratch = scratch;

    scratch = newscratch = malloc(2 * data->set.upload_buffer_size);
    if(!newscratch) {
      failf(data, "Failed to alloc scratch buffer");

      return CURLE_OUT_OF_MEMORY;
    }
  }
  DEBUGASSERT((size_t)data->set.upload_buffer_size >= (size_t)nread);
  ...
}

#define malloc(size) Curl_cmalloc(size)


/* Wrapper: calls through Curl_cfree */
void Curl_cfree_caller(void) {
    Curl_cfree();
}



/* Stub implementation for malloc */
void malloc(size) {}
