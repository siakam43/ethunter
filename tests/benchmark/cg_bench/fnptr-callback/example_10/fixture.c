/* CG-Bench fixture: fnptr-callback/example_10 */
/* fnptr: encrypt_block, targets: aes_encrypt_block */

int ccm_mode_encrypt_contiguous_blocks(ccm_ctx_t *ctx, char *data, size_t length,
    crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*copy_block)(uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
    size_t remainder = length;
	...

	if (length + ctx->ccm_remainder_len < block_size) {
		/* accumulate bytes here and return */
		memcpy((uint8_t *)ctx->ccm_remainder + ctx->ccm_remainder_len,
		    datap,
		    length);
		ctx->ccm_remainder_len += length;
		ctx->ccm_copy_to = datap;
		return (CRYPTO_SUCCESS);
	}

	crypto_init_ptrs(out, &iov_or_mp, &offset);

	mac_buf = (uint8_t *)ctx->ccm_mac_buf;

	do {
		/* Unprocessed data from last call. */
		...

		xor_block(blockp, mac_buf);
		encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

		/* ccm_cb is the counter block */
		encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb,
		    (uint8_t *)ctx->ccm_tmp);

		...
		ctx->ccm_copy_to = NULL;

	} while (remainder > 0);

out:
	return (CRYPTO_SUCCESS);
}

int aes_encrypt_contiguous_blocks(void *ctx, char *data, size_t length,
    crypto_data_t *out)
{
	aes_ctx_t *aes_ctx = ctx;
	int rv;

	if (aes_ctx->ac_flags & CTR_MODE) {
		rv = ctr_mode_contiguous_blocks(ctx, data, length, out,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
	} else if (aes_ctx->ac_flags & CCM_MODE) {
		rv = ccm_mode_encrypt_contiguous_blocks(ctx, data, length,
		    out, AES_BLOCK_LEN, aes_encrypt_block, aes_copy_block,
		    aes_xor_block);
    }
    ...
}


/* Wrapper: calls through encrypt_block */
void encrypt_block_caller(void) {
    encrypt_block();
}



/* Stub implementation for aes_encrypt_block */
void aes_encrypt_block(void) {}
