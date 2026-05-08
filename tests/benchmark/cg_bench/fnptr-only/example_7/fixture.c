/* CG-Bench fixture: fnptr-only/example_7 */
/* fnptr: xfunc, targets: lzjb_decompress, gzip_decompress, zle_decompress, lz4_decompress_zfs, zfs_zstd_decompress */

int
zstream_do_decompress(int argc, char *argv[])
{
	...

	while (sfread(drr, sizeof (*drr), stdin) != 0) {
		struct drr_write *drrw;
		uint64_t payload_size = 0;

		/*
		 * We need to regenerate the checksum.
		 */
		if (drr->drr_type != DRR_BEGIN) {
			memset(&drr->drr_u.drr_checksum.drr_checksum, 0,
			    sizeof (drr->drr_u.drr_checksum.drr_checksum));
		}

		switch (drr->drr_type) {
		...
		case DRR_WRITE_BYREF:
			VERIFY3S(begin, ==, 1);
			fprintf(stderr,
			    "Deduplicated streams are not supported\n");
			exit(1);
			break;

		case DRR_WRITE:
		{
			VERIFY3S(begin, ==, 1);
			drrw = &thedrr.drr_u.drr_write;
			payload_size = DRR_WRITE_PAYLOAD_SIZE(drrw);
			ENTRY *p;
			char key[KEYSIZE];

			snprintf(key, KEYSIZE, "%llu,%llu",
			    (u_longlong_t)drrw->drr_object,
			    (u_longlong_t)drrw->drr_offset);
			ENTRY e = {.key = key};

			p = hsearch(e, FIND);
			if (p != NULL) {
				zio_decompress_func_t *xfunc = NULL;
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
					assert(B_FALSE);
				}


				/*
				 * Read and decompress the block
				 */
				char *lzbuf = safe_calloc(payload_size);
				(void) sfread(lzbuf, payload_size, stdin);
				if (xfunc == NULL) {
					memcpy(buf, lzbuf, payload_size);
					drrw->drr_compressiontype =
					    ZIO_COMPRESS_OFF;
					if (verbose)
						fprintf(stderr, "Resetting "
						    "compression type to off "
						    "for ino %llu offset "
						    "%llu\n",
						    (u_longlong_t)
						    drrw->drr_object,
						    (u_longlong_t)
						    drrw->drr_offset);
				} else if (0 != xfunc(lzbuf, buf,
				    payload_size, payload_size, 0)) {
					/*
					 * The block must not be compressed,
					 * at least not with this compression
					 * type, possibly because it gets
					 * written multiple times in this
					 * stream.
					 */
					warnx("decompression failed for "
					    "ino %llu offset %llu",
					    (u_longlong_t)drrw->drr_object,
					    (u_longlong_t)drrw->drr_offset);
					memcpy(buf, lzbuf, payload_size);
				} else if (verbose) {
					drrw->drr_compressiontype =
					    ZIO_COMPRESS_OFF;
					fprintf(stderr, "successfully "
					    "decompressed ino %llu "
					    "offset %llu\n",
					    (u_longlong_t)drrw->drr_object,
					    (u_longlong_t)drrw->drr_offset);
				} else {
					drrw->drr_compressiontype =
					    ZIO_COMPRESS_OFF;
				}
				free(lzbuf);
			} else {
				/*
				 * Read the contents of the block unaltered
				 */
				(void) sfread(buf, payload_size, stdin);
			}
			break;
		}

		case DRR_WRITE_EMBEDDED:
		{
			VERIFY3S(begin, ==, 1);
			struct drr_write_embedded *drrwe =
			    &drr->drr_u.drr_write_embedded;
			payload_size =
			    P2ROUNDUP((uint64_t)drrwe->drr_psize, 8);
			(void) sfread(buf, payload_size, stdin);
			break;
		}

		case DRR_FREEOBJECTS:
		case DRR_FREE:
		case DRR_OBJECT_RANGE:
			VERIFY3S(begin, ==, 1);
			break;

		default:
			(void) fprintf(stderr, "INVALID record type 0x%x\n",
			    drr->drr_type);
			/* should never happen, so assert */
			assert(B_FALSE);
		}
		...
	}
	free(buf);
	fletcher_4_fini();
	hdestroy();

	return (0);
}


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
