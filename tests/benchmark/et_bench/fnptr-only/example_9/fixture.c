/* ET-Bench fixture: fnptr-only/example_9 */
/* fnptr: Curl_cfree, targets: free */
/* Source: curl-style digest authentication */

#include <stdlib.h>
#include <string.h>

typedef void (*curl_free_callback)(void *ptr);
typedef enum {
    CURLE_OK = 0,
    CURLE_OUT_OF_MEMORY,
    CURLE_BAD_CONTENT_ENCODING
} CURLcode;

curl_free_callback Curl_cfree = (curl_free_callback)free;

struct Curl_easy {
    char *userpwd;
    char *proxyuserpwd;
};

#define Curl_safefree(ptr) \
    do { Curl_cfree((ptr)); (ptr) = NULL; } while(0)

CURLcode Curl_output_digest(struct Curl_easy *data,
                            int proxy,
                            const unsigned char *request,
                            const unsigned char *uripath)
{
    char **allocuserpwd = proxy ? &data->proxyuserpwd : &data->userpwd;
    char *userp = *allocuserpwd;
    char *passwdp = NULL;
    CURLcode result;

    Curl_safefree(*allocuserpwd);

    if (!userp)
        userp = "";

    if (!passwdp)
        passwdp = "";

    *allocuserpwd = malloc(strlen(userp) + strlen(passwdp) + 64);
    if (!*allocuserpwd)
        return CURLE_OUT_OF_MEMORY;

    return CURLE_OK;
}
