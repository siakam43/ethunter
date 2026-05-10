/* ET-Bench fixture: fnptr-only/example_2 */
/* fnptr: Curl_ccalloc, targets: calloc */
/* Source: curl-style memory functions */

#include <stdlib.h>
#include <string.h>

typedef enum {
    CURLE_OK = 0,
    CURLE_OUT_OF_MEMORY
} CURLcode;

typedef struct altsvc {
    char *src_host;
    char *dst_host;
    unsigned int src_port;
    unsigned int dst_port;
    int alpnid_src;
    int alpnid_dst;
} altsvc_t;

typedef void *(*curl_calloc_callback)(size_t nmemb, size_t size);

curl_calloc_callback Curl_ccalloc = (curl_calloc_callback)calloc;

static const char *Curl_new(int srcalpnid, int dstalpnid,
                            unsigned int srcport, unsigned int dstport)
{
    altsvc_t *as = Curl_ccalloc(1, sizeof(altsvc_t));
    if (!as)
        return NULL;

    as->src_port = srcport;
    as->dst_port = dstport;
    as->alpnid_src = srcalpnid;
    as->alpnid_dst = dstalpnid;

    as->src_host = malloc(64);
    if (!as->src_host) {
        free(as);
        return NULL;
    }

    return as->src_host;
}

CURLcode Curl_global_init(long flags)
{
    static int initialized = 0;

    if (initialized++)
        return CURLE_OK;

    if (flags & 1) {
        Curl_ccalloc = (curl_calloc_callback)calloc;
    }

    return CURLE_OK;
}
