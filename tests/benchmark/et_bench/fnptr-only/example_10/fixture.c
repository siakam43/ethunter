/* ET-Bench fixture: fnptr-only/example_10 */
/* fnptr: Curl_cmalloc, targets: malloc */
/* Source: curl-style SMTP escaping */

#include <stdlib.h>
#include <string.h>

typedef void *(*curl_malloc_callback)(size_t size);
typedef enum {
    CURLE_OK = 0,
    CURLE_OUT_OF_MEMORY
} CURLcode;

curl_malloc_callback Curl_cmalloc = (curl_malloc_callback)malloc;

struct Curl_easy {
    int upload_buffer_size;
    int crlf;
};

void failf(struct Curl_easy *data, const char *fmt) {}

CURLcode Curl_smtp_escape_eob(struct Curl_easy *data,
                              const int nread,
                              const int offset)
{
    char *scratch = NULL;
    char *newscratch;
    char *oldscratch;
    size_t bufsize;

    if (!scratch || data->crlf) {
        oldscratch = scratch;

        scratch = newscratch = Curl_cmalloc(2 * data->upload_buffer_size);
        if (!newscratch) {
            failf(data, "Failed to alloc scratch buffer");
            return CURLE_OUT_OF_MEMORY;
        }
    }

    if ((size_t)data->upload_buffer_size >= (size_t)nread) {
        bufsize = nread;
    } else {
        bufsize = data->upload_buffer_size;
    }

    if (scratch) {
        Curl_cmalloc(64);
    }

    return CURLE_OK;
}
