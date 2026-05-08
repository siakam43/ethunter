/* CG-Bench fixture: fnptr-only/example_8 */
/* fnptr: Curl_cfree, targets: free */

struct Cookie *
Curl_cookie_add(struct Curl_easy *data,
                struct CookieInfo *c,
                bool httpheader, /* TRUE if HTTP header-style line */
                bool noexpire, /* if TRUE, skip remove_expired() */
                const char *lineptr,   /* first character of the line */
                const char *domain, /* default domain */
                const char *path,   /* full path used when this cookie is set,
                                       used to get default path for the cookie
                                       unless set */
                bool secure)  /* TRUE if connection is over secure origin */
{
    ...
    if(lineptr[0]=='#') {
      /* don't even try the comments */
      free(co);
      return NULL;
    }
    ...
}

#define free(ptr) Curl_cfree(ptr)


/* Wrapper: calls through Curl_cfree */
void Curl_cfree_caller(void) {
    Curl_cfree();
}



/* Stub implementation for free */
void free(ptr) {}
