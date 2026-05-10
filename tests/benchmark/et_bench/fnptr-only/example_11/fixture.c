/* ET-Bench fixture: fnptr-only/example_11 */
/* fnptr: Curl_cstrdup, targets: Curl_strdup, strdup */
/* Source: curl-style cookie string handling */

#include <stdlib.h>
#include <string.h>

typedef char *(*curl_strdup_callback)(const char *str);

curl_strdup_callback Curl_cstrdup;

/* Initialize strdup implementation: runtime flag allows both builtin and libc paths */
void Curl_init_strdup(int use_builtin)
{
    if (use_builtin)
        Curl_cstrdup = Curl_strdup;
    else
        Curl_cstrdup = strdup;
}

char *Curl_strdup(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *dup = malloc(len);
    if (dup) {
        memcpy(dup, str, len);
    }
    return dup;
}

struct Cookie {
    char *name;
    char *value;
    char *domain;
    char *path;
};

struct CookieInfo {
    struct Cookie *cookies;
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
    char *tok_buf;
    char *ptr;
    char *firstptr;
    int fields;

    co = calloc(1, sizeof(struct Cookie));
    if (!co)
        return NULL;

    firstptr = strdup(lineptr);
    for (ptr = firstptr, fields = 0; ptr; ptr = NULL, fields++) {
        switch (fields) {
        case 0:
            co->domain = Curl_cstrdup(ptr);
            break;
        case 1:
            co->path = Curl_cstrdup(ptr);
            break;
        case 5:
            co->name = Curl_cstrdup(ptr);
            break;
        default:
            break;
        }
        if (fields > 2) break;
    }

    free(firstptr);
    return co;
}
