/* ET-Bench fixture: fnptr-global-struct-array/example_3 */
/* fnptr: mappings[i].writefunc, targets: writeLong, writeOffset, writeString, writeTime */
/* Pattern: global array of writeout variable structs with per-variable output function pointer */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TRUE 1
#define FALSE 0
#define CURLINFO_NONE 0
#define CURLINFO_CONNECT_TIME_T 0x100
#define CURLINFO_NAMELOOKUP_TIME_T 0x101
#define CURLINFO_TOTAL_TIME_T 0x102
#define CURLINFO_SIZE_DOWNLOAD_T 0x200
#define CURLINFO_PROXY_SSL_VERIFYRESULT 0x300
#define CURLINFO_CERT 0x400
#define CURLINFO_SCHEME 0x500
#define CURLINFO_INPUT_URL 0x600
#define CURLINFO_INPUT_URLSCHEME 0x601
#define CURLINFO_INPUT_URLUSER 0x602
#define CURLINFO_INPUT_URLPASSWORD 0x603
#define CURLINFO_ONERROR 0x700
#define CURLINFO_SIZE_UPLOAD_T 0x201

typedef int CURLINFO;
typedef int CURLcode;
#define CURLE_OK 0

struct OperationConfig {
    const char *writeout;
};

struct per_transfer {
    void *curl;
};

typedef int writefunc_fn(FILE *stream, const void *mapping,
                         struct per_transfer *per, CURLcode per_result, bool done);

struct writeoutvar {
    const char *name;
    int id;
    CURLINFO info;
    writefunc_fn *writefunc;
};

void jsonWriteString(FILE *stream, const char *str, bool done) {
    fprintf(stream, "\"%s\"", str ? str : "");
    if (done) fputc(',', stream);
}

/* Target functions */
static int writeLong(FILE *stream, const void *mapping, struct per_transfer *per, CURLcode per_result, bool done) {
    (void)mapping; (void)per; (void)per_result;
    fprintf(stream, "0");
    if (done) fputc(',', stream);
    return 1;
}

static int writeOffset(FILE *stream, const void *mapping, struct per_transfer *per, CURLcode per_result, bool done) {
    (void)mapping; (void)per; (void)per_result;
    fprintf(stream, "0");
    if (done) fputc(',', stream);
    return 1;
}

static int writeString(FILE *stream, const void *mapping, struct per_transfer *per, CURLcode per_result, bool done) {
    (void)mapping; (void)per; (void)per_result;
    jsonWriteString(stream, "unknown", done);
    return 1;
}

static int writeTime(FILE *stream, const void *mapping, struct per_transfer *per, CURLcode per_result, bool done) {
    (void)mapping; (void)per; (void)per_result;
    fprintf(stream, "0.0");
    if (done) fputc(',', stream);
    return 1;
}

static const struct writeoutvar variables[] = {
    {"certs", 0, CURLINFO_CERT, writeString},
    {"onerror", 0, CURLINFO_ONERROR, NULL},
    {"proxy_ssl_verify_result", 0, CURLINFO_PROXY_SSL_VERIFYRESULT, writeLong},
    {"scheme", 0, CURLINFO_SCHEME, writeString},
    {"size_download", 0, CURLINFO_SIZE_DOWNLOAD_T, writeOffset},
    {"size_upload", 0, CURLINFO_SIZE_UPLOAD_T, writeOffset},
    {"time_connect", 0, CURLINFO_CONNECT_TIME_T, writeTime},
    {"time_namelookup", 0, CURLINFO_NAMELOOKUP_TIME_T, writeTime},
    {"time_total", 0, CURLINFO_TOTAL_TIME_T, writeTime},
    {"url", 0, CURLINFO_INPUT_URL, writeString},
    {"url.scheme", 0, CURLINFO_INPUT_URLSCHEME, writeString},
    {"url.user", 0, CURLINFO_INPUT_URLUSER, writeString},
    {"url.password", 0, CURLINFO_INPUT_URLPASSWORD, writeString},
    {NULL, 0, CURLINFO_NONE, NULL}
};

const char *curl_version(void) { return "7.0"; }

void ourWriteOutJSON(FILE *stream, const struct writeoutvar mappings[],
                     struct per_transfer *per, CURLcode per_result)
{
    int i;
    fputs("{", stream);
    for (i = 0; mappings[i].name != NULL; i++) {
        if (mappings[i].writefunc &&
            mappings[i].writefunc(stream, &mappings[i], per, per_result, TRUE))
            fputc(' ', stream);
    }
    fprintf(stream, "\"curl_version\":");
    jsonWriteString(stream, curl_version(), FALSE);
    fprintf(stream, "}");
}

void ourWriteOut(struct OperationConfig *config, struct per_transfer *per,
                 CURLcode per_result)
{
    FILE *stream = stdout;
    const char *ptr = config->writeout;
    (void)ptr; (void)per; (void)per_result;
    ourWriteOutJSON(stream, variables, per, per_result);
}
