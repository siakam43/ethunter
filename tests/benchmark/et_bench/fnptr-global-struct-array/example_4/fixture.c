/* ET-Bench fixture: fnptr-global-struct-array/example_4 */
/* fnptr: dinfo->ci_decompress, targets: lzjb_decompress, gzip_decompress, zle_decompress, lz4_decompress_zfs, zfs_zstd_decompress */
/* Pattern: global compression algorithm table with decompress function pointer per algorithm */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ZIO_COMPRESS_FUNCTIONS 16

typedef int zio_decompress_func_t(const void *src, void *dst, size_t src_len, size_t dst_len, int level);

typedef const struct zio_compress_info {
    const char *ci_name;
    int ci_level;
    void *ci_compress;
    zio_decompress_func_t *ci_decompress;
    void *ci_decompress_level;
} zio_compress_info_t;

/* Target decompression functions */
static int lzjb_decompress(const void *src, void *dst, size_t src_len, size_t dst_len, int level) {
    (void)src; (void)dst; (void)src_len; (void)dst_len; (void)level;
    return 0;
}

static int gzip_decompress(const void *src, void *dst, size_t src_len, size_t dst_len, int level) {
    (void)src; (void)dst; (void)src_len; (void)dst_len; (void)level;
    return 0;
}

static int zle_decompress(const void *src, void *dst, size_t src_len, size_t dst_len, int level) {
    (void)src; (void)dst; (void)src_len; (void)dst_len; (void)level;
    return 0;
}

static int lz4_decompress_zfs(const void *src, void *dst, size_t src_len, size_t dst_len, int level) {
    (void)src; (void)dst; (void)src_len; (void)dst_len; (void)level;
    return 0;
}

static int zfs_zstd_decompress(const void *src, void *dst, size_t src_len, size_t dst_len, int level) {
    (void)src; (void)dst; (void)src_len; (void)dst_len; (void)level;
    return 0;
}

static int lzjb_compress(const void *s, void *d, size_t sl, size_t dl, int l) { (void)s; (void)d; (void)sl; (void)dl; (void)l; return 0; }
static int gzip_compress(const void *s, void *d, size_t sl, size_t dl, int l) { (void)s; (void)d; (void)sl; (void)dl; (void)l; return 0; }
static int zle_compress(const void *s, void *d, size_t sl, size_t dl, int l) { (void)s; (void)d; (void)sl; (void)dl; (void)l; return 0; }
static int lz4_compress_zfs(const void *s, void *d, size_t sl, size_t dl, int l) { (void)s; (void)d; (void)sl; (void)dl; (void)l; return 0; }
static int zfs_zstd_compress_wrap(const void *s, void *d, size_t sl, size_t dl, int l) { (void)s; (void)d; (void)sl; (void)dl; (void)l; return 0; }

#define ZIO_ZSTD_LEVEL_DEFAULT 3

/* Global compression table */
zio_compress_info_t zio_compress_table[ZIO_COMPRESS_FUNCTIONS] = {
    {"inherit",          0, NULL, NULL, NULL},
    {"on",               0, NULL, NULL, NULL},
    {"uncompressed",     0, NULL, NULL, NULL},
    {"lzjb",             0, lzjb_compress, lzjb_decompress, NULL},
    {"empty",            0, NULL, NULL, NULL},
    {"gzip-1",           1, gzip_compress, gzip_decompress, NULL},
    {"gzip-2",           2, gzip_compress, gzip_decompress, NULL},
    {"gzip-9",           9, gzip_compress, gzip_decompress, NULL},
    {"zle",              64, zle_compress, zle_decompress, NULL},
    {"lz4",              0, lz4_compress_zfs, lz4_decompress_zfs, NULL},
    {"zstd",             ZIO_ZSTD_LEVEL_DEFAULT, zfs_zstd_compress_wrap,
     zfs_zstd_decompress, NULL},
};

int zstream_do_recompress(int argc, char *argv[])
{
    int bufsz = 65536;
    char *buf = (char *)malloc(bufsz);
    char *dbuf = (char *)malloc(bufsz);
    char *cbuf = (char *)malloc(bufsz);
    int payload_size = 100;
    int level = -1;
    zio_compress_info_t *dinfo = &zio_compress_table[3]; /* lzjb */

    (void)argc; (void)argv; (void)level;

    if (dinfo->ci_decompress != NULL) {
        if (0 != dinfo->ci_decompress(cbuf, dbuf,
            payload_size, bufsz, dinfo->ci_level)) {
            fprintf(stderr, "Decompression failed\n");
        }
    }

    free(buf); free(dbuf); free(cbuf);
    return 0;
}
