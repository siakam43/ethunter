/* CG-Bench fixture: fnptr-global-struct-array/example_4 */
/* fnptr: ci_decompress, targets: lzjb_decompress, gzip_decompress, zle_decompress, lz4_decompress_zfs, zfs_zstd_decompress */

int zstream_do_recompress(int argc, char *argv[])
{
	int bufsz = SPA_MAXBLOCKSIZE;
	char *buf = safe_malloc(bufsz);
	dmu_replay_record_t thedrr;
	dmu_replay_record_t *drr = &thedrr;
	zio_cksum_t stream_cksum;
	int c;
	int level = -1;
    ...
                    (void) sfread(cbuf, payload_size, stdin);
                    if (dinfo->ci_decompress != NULL) {
	                  if (0 != dinfo->ci_decompress(cbuf, dbuf,
				        payload_size, MIN(bufsz,
				        drrw->drr_logical_size), dinfo->ci_level))
    ...
                    }
}

/*
 * Compression vectors.
 */
zio_compress_info_t zio_compress_table[ZIO_COMPRESS_FUNCTIONS] = {
	{"inherit",	0,	NULL,		NULL, NULL},
	{"on",		0,	NULL,		NULL, NULL},
	{"uncompressed", 0,	NULL,		NULL, NULL},
	{"lzjb",	0,	lzjb_compress,	lzjb_decompress, NULL},
	{"empty",	0,	NULL,		NULL, NULL},
	{"gzip-1",	1,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-2",	2,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-3",	3,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-4",	4,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-5",	5,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-6",	6,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-7",	7,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-8",	8,	gzip_compress,	gzip_decompress, NULL},
	{"gzip-9",	9,	gzip_compress,	gzip_decompress, NULL},
	{"zle",		64,	zle_compress,	zle_decompress, NULL},
	{"lz4",		0,	lz4_compress_zfs, lz4_decompress_zfs, NULL},
	{"zstd",	ZIO_ZSTD_LEVEL_DEFAULT,	zfs_zstd_compress_wrap,
	    zfs_zstd_decompress, zfs_zstd_decompress_level},
};

typedef const struct zio_compress_info {
	const char			*ci_name;
	int				ci_level;
	zio_compress_func_t		*ci_compress;
	zio_decompress_func_t		*ci_decompress;
	zio_decompresslevel_func_t	*ci_decompress_level;
} zio_compress_info_t;


/* Stub implementation for lzjb_decompress */
void lzjb_decompress(void) {}



/* Stub implementation for gzip_decompress */
void gzip_decompress(void) {}



/* Stub implementation for zle_decompress */
void zle_decompress(void) {}



/* Stub implementation for lz4_decompress_zfs */
void lz4_decompress_zfs(void) {}



/* Stub implementation for zfs_zstd_decompress */
void zfs_zstd_decompress(void) {}
