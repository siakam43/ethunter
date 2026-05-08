/* CG-Bench fixture: fnptr-only/example_11 */
/* fnptr: strdup, targets: Curl_strdup, strdup */

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
    for(ptr = firstptr, fields = 0; ptr && !badcookie;
        ptr = strtok_r(NULL, "\t", &tok_buf), fields++) {
        switch(fields) {
        ...
        case 5:
        co->name = strdup(ptr);
        ...
        }
    }
}

#define strdup(ptr) Curl_cstrdup(ptr)

curl_strdup_callback Curl_cstrdup = (curl_strdup_callback)system_strdup;

#if defined(_WIN32_WCE)
...
#elif !defined(HAVE_STRDUP)
#define system_strdup Curl_strdup
#else
...
#endif


/* Wrapper: calls through strdup */
void strdup_caller(void) {
    strdup();
}



/* Stub implementation for Curl_strdup */
void Curl_strdup(void) {}



/* Stub implementation for strdup */
void strdup(ptr) {}
