/* ET-Bench fixture: fnptr-only/example_8 */
/* fnptr: Curl_cfree, targets: free */
/* Source: curl-style cookie parser */

#include <stdlib.h>
#include <string.h>

typedef void (*curl_free_callback)(void *ptr);

curl_free_callback Curl_cfree = (curl_free_callback)free;

struct Cookie {
    char *name;
    char *value;
    char *domain;
    char *path;
    struct Cookie *next;
};

struct CookieInfo {
    struct Cookie *cookies;
    int num_cookies;
};

struct Curl_easy {
    int dummy;
};

struct Cookie *
Curl_cookie_add(struct Curl_easy *data,
                struct CookieInfo *c,
                int httpheader,
                int noexpire,
                const char *lineptr,
                const char *domain,
                const char *path,
                int secure)
{
    struct Cookie *co;

    co = calloc(1, sizeof(struct Cookie));
    if (!co)
        return NULL;

    if (lineptr[0] == '#') {
        Curl_cfree(co);
        return NULL;
    }

    co->name = strdup(lineptr);
    co->value = strdup(lineptr + 1);
    co->domain = strdup(domain);
    co->path = strdup(path);
    co->next = NULL;

    return co;
}
