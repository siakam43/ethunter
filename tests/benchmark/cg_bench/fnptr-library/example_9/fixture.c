/* CG-Bench fixture: fnptr-library/example_9 */
/* fnptr: list->dtor, targets: fileinfo_dtor, hash_element_dtor, free_bundle_hash_entry, freednsentry, trhash_dtor, sh_freeentry, curl_free, gsasl_free */

void
Curl_llist_remove(struct Curl_llist *list, struct Curl_llist_element *e,
                  void *user)
{
  void *ptr;
  if(!e || list->size == 0)
    return;

  ...
  --list->size;

  /* call the dtor() last for when it actually frees the 'e' memory itself */
  if(list->dtor)
    list->dtor(user, ptr);
}

void Curl_bufref_set(struct bufref *br, const void *ptr, size_t len,
                     void (*dtor)(void *))
{
  DEBUGASSERT(ptr || !len);
  DEBUGASSERT(len <= CURL_MAX_INPUT_LENGTH);

  Curl_bufref_free(br);
  br->ptr = (const unsigned char *) ptr;
  br->len = len;
  br->dtor = dtor;
}

static CURLcode init_wc_data(struct Curl_easy *data)
{
    ...
    wildcard->ftpwc = ftpwc; /* put it to the WildcardData tmp pointer */
    wildcard->dtor = wc_data_dtor;

    ...

fail:
    if(ftpwc) {
        Curl_ftp_parselist_data_free(&ftpwc->parser);
        free(ftpwc);
    }
    Curl_safefree(wildcard->pattern);
    wildcard->dtor = ZERO_NULL;
    wildcard->ftpwc = NULL;
    return result;
}

#define ZERO_NULL 0

void
Curl_hash_init(struct Curl_hash *h,
               int slots,
               hash_function hfunc,
               comp_function comparator,
               Curl_hash_dtor dtor)
{
  DEBUGASSERT(h);
  DEBUGASSERT(slots);
  DEBUGASSERT(hfunc);
  DEBUGASSERT(comparator);
  DEBUGASSERT(dtor);

  h->table = NULL;
  h->hash_func = hfunc;
  h->comp_func = comparator;
  h->dtor = dtor;
  h->size = 0;
  h->slots = slots;
}

void
Curl_llist_init(struct Curl_llist *l, Curl_llist_dtor dtor)
{
  l->size = 0;
  l->dtor = dtor;
  l->head = NULL;
  l->tail = NULL;
}

CURLcode Curl_wildcard_init(struct WildcardData *wc)
{
  Curl_llist_init(&wc->filelist, fileinfo_dtor);
  wc->state = CURLWC_INIT;

  return CURLE_OK;
}

void *
Curl_hash_add(struct Curl_hash *h, void *key, size_t key_len, void *p)
{
  struct Curl_hash_element  *he;
  struct Curl_llist_element *le;
  struct Curl_llist *l;

  DEBUGASSERT(h);
  DEBUGASSERT(h->slots);
  if(!h->table) {
    int i;
    h->table = malloc(h->slots * sizeof(struct Curl_llist));
    if(!h->table)
      return NULL; /* OOM */
    for(i = 0; i < h->slots; ++i)
      Curl_llist_init(&h->table[i], hash_element_dtor);
  }
  ...
}

int Curl_conncache_init(struct conncache *connc, int size)
{
  /* allocate a new easy handle to use when closing cached connections */
  connc->closure_handle = curl_easy_init();
  if(!connc->closure_handle)
    return 1; /* bad */
  connc->closure_handle->state.internal = true;

  Curl_hash_init(&connc->hash, size, Curl_hash_str,
                 Curl_str_key_compare, free_bundle_hash_entry);
  connc->closure_handle->state.conn_cache = connc;

  return 0; /* good */
}

void Curl_init_dnscache(struct Curl_hash *hash, int size)
{
  Curl_hash_init(hash, size, Curl_hash_str, Curl_str_key_compare,
                 freednsentry);
}

static struct Curl_sh_entry *sh_addentry(struct Curl_hash *sh,
                                         curl_socket_t s)
{
  struct Curl_sh_entry *there = sh_getentry(sh, s);
  struct Curl_sh_entry *check;

  if(there) {
    /* it is present, return fine */
    return there;
  }

  /* not present, add it */
  check = calloc(1, sizeof(struct Curl_sh_entry));
  if(!check)
    return NULL; /* major failure */

  Curl_hash_init(&check->transfers, TRHASH_SIZE, trhash, trhash_compare,
                 trhash_dtor);

  /* make/add new hash entry */
  if(!Curl_hash_add(sh, (char *)&s, sizeof(curl_socket_t), check)) {
    Curl_hash_destroy(&check->transfers);
    free(check);
    return NULL; /* major failure */
  }

  return check; /* things are good in sockhash land */
}

static void sh_init(struct Curl_hash *hash, int hashsize)
{
  Curl_hash_init(hash, hashsize, hash_fd, fd_key_compare,
                 sh_freeentry);
}

static CURLcode get_server_message(struct SASL *sasl, struct Curl_easy *data,
                                   struct bufref *out)
{
  CURLcode result = CURLE_OK;

  result = sasl->params->getmessage(data, out);
  if(!result && (sasl->params->flags & SASL_FLAG_BASE64)) {
    unsigned char *msg;
    size_t msglen;
    const char *serverdata = (const char *) Curl_bufref_ptr(out);

    if(!*serverdata || *serverdata == '=')
      Curl_bufref_set(out, NULL, 0, NULL);
    else {
      result = Curl_base64_decode(serverdata, &msg, &msglen);
      if(!result)
        Curl_bufref_set(out, msg, msglen, curl_free);
    }
  }
  return result;
}

CURLcode Curl_auth_gsasl_token(struct Curl_easy *data,
                               const struct bufref *chlg,
                               struct gsasldata *gsasl,
                               struct bufref *out)
{
  int res;
  char *response;
  size_t outlen;

  res = gsasl_step(gsasl->client,
                   (const char *) Curl_bufref_ptr(chlg), Curl_bufref_len(chlg),
                   &response, &outlen);
  if(res != GSASL_OK && res != GSASL_NEEDS_MORE) {
    failf(data, "GSASL step: %s\n", gsasl_strerror(res));
    return CURLE_BAD_CONTENT_ENCODING;
  }

  Curl_bufref_set(out, response, outlen, gsasl_free);
  return CURLE_OK;
}


/* Stub implementation for fileinfo_dtor */
void fileinfo_dtor(void) {}



/* Stub implementation for hash_element_dtor */
void hash_element_dtor(void) {}



/* Stub implementation for free_bundle_hash_entry */
void free_bundle_hash_entry(void) {}



/* Stub implementation for freednsentry */
void freednsentry(void) {}



/* Stub implementation for trhash_dtor */
void trhash_dtor(void) {}



/* Stub implementation for sh_freeentry */
void sh_freeentry(void) {}



/* Stub implementation for curl_free */
void curl_free(void) {}



/* Stub implementation for gsasl_free */
void gsasl_free(void) {}
