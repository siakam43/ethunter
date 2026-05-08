/* CG-Bench fixture: fnptr-only/example_3 */
/* fnptr: md_final_raw, targets: tls1_md5_final_raw, tls1_sha1_final_raw, tls1_sha256_final_raw, tls1_sha512_final_raw */

int unsigned char *md_out,
                           size_t *md_out_size,
                           const unsigned char header[13],
                           const unsigned char *data,
                           size_t data_plus_mac_size,
                           size_t data_plus_mac_plus_padding_size,
                           const unsigned char *mac_secret,
                           size_t mac_secret_length, char is_sslv3)
{
    union {
        double align;
        unsigned char c[sizeof(LARGEST_DIGEST_CTX)];
    } md_state;
    void (*md_final_raw) (void *ctx, unsigned char *md_out);
    void (*md_transform) (void *ctx, const unsigned char *block);
...
switch (EVP_MD_CTX_type(ctx)) {
case NID_md5: 
...
    md_final_raw = tls1_md5_final_raw;
    ...
    break;
case NID_sha1:
...
    md_final_raw = tls1_sha1_final_raw;
    md_transform =
        (void (*)(void *ctx, const unsigned char *block))SHA1_Transform;
    md_size = 20;
    break;
case NID_sha224:
...
    md_final_raw = tls1_sha256_final_raw;
    md_transform =
        (void (*)(void *ctx, const unsigned char *block))SHA256_Transform;
...
    break;
case NID_sha256:
    if (SHA256_Init((SHA256_CTX *)md_state.c) <= 0)
        return 0;
    md_final_raw = tls1_sha256_final_raw;
    md_transform =
        (void (*)(void *ctx, const unsigned char *block))SHA256_Transform;
    md_size = 32;
    break;
case NID_sha384:
...
    md_final_raw = tls1_sha512_final_raw;
    md_transform =
        (void (*)(void *ctx, const unsigned char *block))SHA512_Transform;
...
    break;
case NID_sha512:
...
    md_final_raw = tls1_sha512_final_raw;
...
    break;
default:
    ...

for (i = num_starting_blocks; i <= num_starting_blocks + variance_blocks;
     i++) {
...
    md_transform(md_state.c, block);
    md_final_raw(md_state.c, block);
    /* If this is index_b, copy the hash value to |mac_out|. */
    for (j = 0; j < md_size; j++)
        mac_out[j] |= block[j] & is_block_b;
...
     }
}
}


/* Wrapper: calls through md_final_raw */
void md_final_raw_caller(void *ctx, unsigned char *md_out) {
    md_final_raw(ctx, md_out);
}



/* Stub implementation for tls1_md5_final_raw */
void tls1_md5_final_raw(void) {}



/* Stub implementation for tls1_sha1_final_raw */
void tls1_sha1_final_raw(void) {}



/* Stub implementation for tls1_sha256_final_raw */
void tls1_sha256_final_raw(void) {}



/* Stub implementation for tls1_sha512_final_raw */
void tls1_sha512_final_raw(void) {}
