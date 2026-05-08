/* CG-Bench fixture: fnptr-varargs/example_1 */
/* fnptr: share->lockfunc, targets: my_lock */

CURLSHcode
curl_share_cleanup(struct Curl_share *share)
{
  if(!GOOD_SHARE_HANDLE(share))
    return CURLSHE_INVALID;

  if(share->lockfunc)
    share->lockfunc(NULL, CURL_LOCK_DATA_SHARE, CURL_LOCK_ACCESS_SINGLE,
                    share->clientdata);
  ...
}

int main(void)
{
  CURLSH *share;
  int i;

  share = curl_share_init();
  curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);

  curl_share_setopt(share, CURLSHOPT_LOCKFUNC, my_lock);
  curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, my_unlock);

  /* Loop the transfer and cleanup the handle properly every lap. This will
     still reuse connections since the pool is in the shared object! */

  for(i = 0; i < 3; i++) {
    CURL *curl = curl_easy_init();
    if(curl) {
      CURLcode res;

      curl_easy_setopt(curl, CURLOPT_URL, "https://curl.se/");

      /* use the share object */
      curl_easy_setopt(curl, CURLOPT_SHARE, share);

      /* Perform the request, res will get the return code */
      res = curl_easy_perform(curl);
      /* Check for errors */
      if(res != CURLE_OK)
        fprintf(stderr, "curl_easy_perform() failed: %s\n",
                curl_easy_strerror(res));

      /* always cleanup */
      curl_easy_cleanup(curl);
    }
  }

  curl_share_cleanup(share);
  return 0;
}

CURLSHcode curl_share_setopt(struct Curl_share *share, CURLSHoption option, ...)
{
  va_list param;
  int type;
  curl_lock_function lockfunc;
  curl_unlock_function unlockfunc;
  void *ptr;
  CURLSHcode res = CURLSHE_OK;

  if(!GOOD_SHARE_HANDLE(share))
    return CURLSHE_INVALID;

  if(share->dirty)
    /* don't allow setting options while one or more handles are already
       using this share */
    return CURLSHE_IN_USE;

  va_start(param, option);

  switch(option) {
  case CURLSHOPT_SHARE:
    /* this is a type this share will share */
    type = va_arg(param, int);

    switch(type) {
    case CURL_LOCK_DATA_DNS:
      break;

    ...
    }
    ...

  case CURLSHOPT_LOCKFUNC:
    lockfunc = va_arg(param, curl_lock_function);
    share->lockfunc = lockfunc;
    break;

  case CURLSHOPT_UNLOCKFUNC:
    unlockfunc = va_arg(param, curl_unlock_function);
    share->unlockfunc = unlockfunc;
    break;

  case CURLSHOPT_USERDATA:
    ptr = va_arg(param, void *);
    share->clientdata = ptr;
    break;

  default:
    res = CURLSHE_BAD_OPTION;
    break;
  }

  va_end(param);

  return res;
}


/* Stub implementation for my_lock */
void my_lock(void) {}
