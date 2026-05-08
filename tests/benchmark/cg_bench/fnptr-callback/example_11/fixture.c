/* CG-Bench fixture: fnptr-callback/example_11 */
/* fnptr: xor_block, targets: aes_xor_block */

int ccm_encrypt_final(ccm_ctx_t *ctx, crypto_data_t *out, size_t block_size,
    int (*encrypt_block)(const void *, const uint8_t *, uint8_t *),
    void (*xor_block)(uint8_t *, uint8_t *))
{
	...

	if (ctx->ccm_remainder_len > 0) {

		...

		/* calculate the CBC MAC */
		xor_block(macp, mac_buf);
		encrypt_block(ctx->ccm_keysched, mac_buf, mac_buf);

		/* calculate the counter mode */
		lastp = (uint8_t *)ctx->ccm_tmp;
		encrypt_block(ctx->ccm_keysched, (uint8_t *)ctx->ccm_cb, lastp);

		/* XOR with counter block */
		for (i = 0; i < ctx->ccm_remainder_len; i++) {
			macp[i] ^= lastp[i];
		}
		ctx->ccm_processed_data_len += ctx->ccm_remainder_len;
	}
}

static int
aes_encrypt_atomic(crypto_mechanism_t *mechanism,
    crypto_key_t *key, crypto_data_t *plaintext, crypto_data_t *ciphertext,
    crypto_spi_ctx_template_t template)
{
	...

	if (ret == CRYPTO_SUCCESS) {
		if (mechanism->cm_type == AES_CCM_MECH_INFO_TYPE) {
			ret = ccm_encrypt_final((ccm_ctx_t *)&aes_ctx,
			    ciphertext, AES_BLOCK_LEN, aes_encrypt_block,
			    aes_xor_block);
        }
    }
    ...
}

static int
aes_encrypt_final(crypto_ctx_t *ctx, crypto_data_t *data)
{
	aes_ctx_t *aes_ctx;
	...

	if (aes_ctx->ac_flags & CTR_MODE) {
		...
	} else if (aes_ctx->ac_flags & CCM_MODE) {
		ret = ccm_encrypt_final((ccm_ctx_t *)aes_ctx, data,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
    }
}

static int
aes_encrypt(crypto_ctx_t *ctx, crypto_data_t *plaintext,
    crypto_data_t *ciphertext)
{
	...
	if (aes_ctx->ac_flags & CCM_MODE) {
		/*
		 * ccm_encrypt_final() will compute the MAC and append
		 * it to existing ciphertext. So, need to adjust the left over
		 * length value accordingly
		 */

		/* order of following 2 lines MUST not be reversed */
		ciphertext->cd_offset = ciphertext->cd_length;
		ciphertext->cd_length = saved_length - ciphertext->cd_length;
		ret = ccm_encrypt_final((ccm_ctx_t *)aes_ctx, ciphertext,
		    AES_BLOCK_LEN, aes_encrypt_block, aes_xor_block);
		if (ret != CRYPTO_SUCCESS) {
			return (ret);
		}
    }
}


/* Wrapper: calls through xor_block */
void xor_block_caller(void) {
    xor_block();
}



/* Stub implementation for aes_xor_block */
void aes_xor_block(void) {}
