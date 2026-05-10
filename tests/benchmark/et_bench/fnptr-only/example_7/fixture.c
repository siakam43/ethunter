/* ET-Bench fixture: fnptr-only/example_7 */
/* fnptr: xfunc, targets: lzjb_decompress, gzip_decompress, zle_decompress, lz4_decompress_zfs, zfs_zstd_decompress */
/* Source: ZFS-style stream decompression */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

typedef enum {
    ZIO_COMPRESS_OFF = 0,
    ZIO_COMPRESS_LZJB = 1,
    ZIO_COMPRESS_GZIP_1 = 2,
    ZIO_COMPRESS_ZLE = 3,
    ZIO_COMPRESS_LZ4 = 4,
    ZIO_COMPRESS_ZSTD = 5
} zio_compress_t;

typedef int (*zio_decompress_func_t)(void *src, void *dst,
                                      size_t csize, size_t psize, int flags);

static int lzjb_decompress(void *src, void *dst,
                            size_t csize, size_t psize, int flags) {
    memcpy(dst, src, psize);
    return 0;
}

static int gzip_decompress(void *src, void *dst,
                            size_t csize, size_t psize, int flags) {
    if (csize == 0) return -1;
    memcpy(dst, src, psize);
    return 0;
}

static int zle_decompress(void *src, void *dst,
                           size_t csize, size_t psize, int flags) {
    memset(dst, *(char *)src, psize);
    return 0;
}

static int lz4_decompress_zfs(void *src, void *dst,
                               size_t csize, size_t psize, int flags) {
    if (csize < 4) return -1;
    memcpy(dst, src, psize);
    return 0;
}

static int zfs_zstd_decompress(void *src, void *dst,
                                size_t csize, size_t psize, int flags) {
    if (csize < 8) return -1;
    memcpy(dst, src, psize);
    return 0;
}

typedef struct {
    int drr_type;
    struct {
        struct {
            unsigned char drr_checksum[32];
        } drr_checksum;
    } drr_u;
} drr_header_t;

typedef struct {
    unsigned long long drr_object;
    unsigned long long drr_offset;
} drr_write_t;

#define DRR_BEGIN 1
#define DRR_WRITE 2
#define DRR_WRITE_BYREF 3
#define DRR_WRITE_EMBEDDED 4
#define DRR_FREEOBJECTS 5
#define DRR_FREE 6
#define DRR_OBJECT_RANGE 7
#define DRR_WRITE_PAYLOAD_SIZE(w) ((size_t)64)

typedef struct {
    char *key;
    void *data;
} ENTRY;

#define FIND 1
static void *hsearch(ENTRY e, int action) { return NULL; }

static char *safe_calloc(size_t sz) { return calloc(1, sz); }

int zstream_do_decompress(int argc, char *argv[])
{
    unsigned char buf[4096];
    drr_header_t drr;
    int verbose = 0;

    while (1) {
        ENTRY *p;
        char key[64];

        int compress_type = ZIO_COMPRESS_LZJB;

        snprintf(key, sizeof(key), "%llu,%llu",
                 (unsigned long long)100, (unsigned long long)0);
        ENTRY e = {.key = key, .data = (void *)(intptr_t)compress_type};

        p = hsearch(e, FIND);
        if (p != NULL) {
            zio_decompress_func_t *xfunc = NULL;
            size_t payload_size = 64;

            switch ((enum zio_compress)(intptr_t)p->data) {
            case ZIO_COMPRESS_OFF:
                xfunc = NULL;
                break;
            case ZIO_COMPRESS_LZJB:
                xfunc = lzjb_decompress;
                break;
            case ZIO_COMPRESS_GZIP_1:
                xfunc = gzip_decompress;
                break;
            case ZIO_COMPRESS_ZLE:
                xfunc = zle_decompress;
                break;
            case ZIO_COMPRESS_LZ4:
                xfunc = lz4_decompress_zfs;
                break;
            case ZIO_COMPRESS_ZSTD:
                xfunc = zfs_zstd_decompress;
                break;
            default:
                assert(0);
            }

            char *lzbuf = safe_calloc(payload_size);
            if (!lzbuf) break;

            if (xfunc == NULL) {
                memcpy(buf, lzbuf, payload_size);
            } else if (0 != xfunc(lzbuf, buf,
                                  payload_size, payload_size, 0)) {
                fprintf(stderr, "decompression failed\n");
                memcpy(buf, lzbuf, payload_size);
            } else if (verbose) {
                fprintf(stderr, "successfully decompressed\n");
            }

            free(lzbuf);
        }

        break;
    }

    return 0;
}
