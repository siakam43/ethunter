/* CG-Bench fixture: fnptr-global-struct-array/example_7 */
/* fnptr: ops->transform, targets: sha256_generic, sha512_generic, tf_sha512_transform_x64, tf_sha256_transform_x64 */

static void sha256_update(sha256_ctx *ctx, const uint8_t *data, size_t len)
{
	uint64_t pos = ctx->count[0];
	uint64_t total = ctx->count[1];
	uint8_t *m = ctx->wbuf;
	const sha256_ops_t *ops = ctx->ops;

	if (pos && pos + len >= 64) {
		memcpy(m + pos, data, 64 - pos);
		ops->transform(ctx->state, m, 1);
		len -= 64 - pos;
		total += (64 - pos) * 8;
		data += 64 - pos;
		pos = 0;
	}

	if (len >= 64) {
		uint32_t blocks = len / 64;
		uint32_t bytes = blocks * 64;
		ops->transform(ctx->state, data, blocks);
		len -= bytes;
		total += (bytes) * 8;
		data += bytes;
	}
	memcpy(m + pos, data, len);

	pos += len;
	total += len * 8;
	ctx->count[0] = pos;
	ctx->count[1] = total;
}

void
SHA2Update(SHA2_CTX *ctx, const void *data, size_t len)
{
	/* check for zero input length */
	if (len == 0)
		return;

	ASSERT3P(data, !=, NULL);

	switch (ctx->algotype) {
		case SHA256_MECH_INFO_TYPE:
		case SHA256_HMAC_MECH_INFO_TYPE:
		case SHA256_HMAC_GEN_MECH_INFO_TYPE:
			sha256_update(&ctx->sha256, data, len);
			break;
		case SHA384_MECH_INFO_TYPE:
		case SHA384_HMAC_MECH_INFO_TYPE:
		case SHA384_HMAC_GEN_MECH_INFO_TYPE:
			sha512_update(&ctx->sha512, data, len);
			break;
		case SHA512_MECH_INFO_TYPE:
		case SHA512_HMAC_MECH_INFO_TYPE:
		case SHA512_HMAC_GEN_MECH_INFO_TYPE:
			sha512_update(&ctx->sha512, data, len);
			break;
		case SHA512_224_MECH_INFO_TYPE:
			sha512_update(&ctx->sha512, data, len);
			break;
		case SHA512_256_MECH_INFO_TYPE:
			sha512_update(&ctx->sha512, data, len);
			break;
	}
}

static void
sha2_mac_init_ctx(sha2_hmac_ctx_t *ctx, void *keyval, uint_t length_in_bytes)
{
	uint64_t ipad[SHA512_HMAC_BLOCK_SIZE / sizeof (uint64_t)] = {0};
	uint64_t opad[SHA512_HMAC_BLOCK_SIZE / sizeof (uint64_t)] = {0};
	int i, block_size, blocks_per_int64;

	/* Determine the block size */
	if (ctx->hc_mech_type <= SHA256_HMAC_GEN_MECH_INFO_TYPE) {
		block_size = SHA256_HMAC_BLOCK_SIZE;
		blocks_per_int64 = SHA256_HMAC_BLOCK_SIZE / sizeof (uint64_t);
	} else {
		block_size = SHA512_HMAC_BLOCK_SIZE;
		blocks_per_int64 = SHA512_HMAC_BLOCK_SIZE / sizeof (uint64_t);
	}

	(void) memset(ipad, 0, block_size);
	(void) memset(opad, 0, block_size);

	if (keyval != NULL) {
		(void) memcpy(ipad, keyval, length_in_bytes);
		(void) memcpy(opad, keyval, length_in_bytes);
	} else {
		ASSERT0(length_in_bytes);
	}

	/* XOR key with ipad (0x36) and opad (0x5c) */
	for (i = 0; i < blocks_per_int64; i ++) {
		ipad[i] ^= 0x3636363636363636;
		opad[i] ^= 0x5c5c5c5c5c5c5c5c;
	}

	/* perform SHA2 on ipad */
	SHA2Init(ctx->hc_mech_type, &ctx->hc_icontext);
	SHA2Update(&ctx->hc_icontext, (uint8_t *)ipad, block_size);

	/* perform SHA2 on opad */
	SHA2Init(ctx->hc_mech_type, &ctx->hc_ocontext);
	SHA2Update(&ctx->hc_ocontext, (uint8_t *)opad, block_size);
}

void
SHA2Init(int algotype, SHA2_CTX *ctx)
{
	sha256_ctx *ctx256 = &ctx->sha256;
	sha512_ctx *ctx512 = &ctx->sha512;

	ASSERT3S(algotype, >=, SHA256_MECH_INFO_TYPE);
	ASSERT3S(algotype, <=, SHA512_256_MECH_INFO_TYPE);

	memset(ctx, 0, sizeof (*ctx));
	ctx->algotype = algotype;
	switch (ctx->algotype) {
		case SHA256_MECH_INFO_TYPE:
		case SHA256_HMAC_MECH_INFO_TYPE:
		case SHA256_HMAC_GEN_MECH_INFO_TYPE:
			ctx256->state[0] = 0x6a09e667;
			ctx256->state[1] = 0xbb67ae85;
			ctx256->state[2] = 0x3c6ef372;
			ctx256->state[3] = 0xa54ff53a;
			ctx256->state[4] = 0x510e527f;
			ctx256->state[5] = 0x9b05688c;
			ctx256->state[6] = 0x1f83d9ab;
			ctx256->state[7] = 0x5be0cd19;
			ctx256->count[0] = 0;
			ctx256->ops = sha256_get_ops();
			break;
		case SHA384_MECH_INFO_TYPE:
		case SHA384_HMAC_MECH_INFO_TYPE:
		case SHA384_HMAC_GEN_MECH_INFO_TYPE:
			ctx512->state[0] = 0xcbbb9d5dc1059ed8ULL;
			ctx512->state[1] = 0x629a292a367cd507ULL;
			ctx512->state[2] = 0x9159015a3070dd17ULL;
			ctx512->state[3] = 0x152fecd8f70e5939ULL;
			ctx512->state[4] = 0x67332667ffc00b31ULL;
			ctx512->state[5] = 0x8eb44a8768581511ULL;
			ctx512->state[6] = 0xdb0c2e0d64f98fa7ULL;
			ctx512->state[7] = 0x47b5481dbefa4fa4ULL;
			ctx512->count[0] = 0;
			ctx512->count[1] = 0;
			ctx512->ops = sha512_get_ops();
			break;
		case SHA512_MECH_INFO_TYPE:
		case SHA512_HMAC_MECH_INFO_TYPE:
		case SHA512_HMAC_GEN_MECH_INFO_TYPE:
			ctx512->state[0] = 0x6a09e667f3bcc908ULL;
			ctx512->state[1] = 0xbb67ae8584caa73bULL;
			ctx512->state[2] = 0x3c6ef372fe94f82bULL;
			ctx512->state[3] = 0xa54ff53a5f1d36f1ULL;
			ctx512->state[4] = 0x510e527fade682d1ULL;
			ctx512->state[5] = 0x9b05688c2b3e6c1fULL;
			ctx512->state[6] = 0x1f83d9abfb41bd6bULL;
			ctx512->state[7] = 0x5be0cd19137e2179ULL;
			ctx512->count[0] = 0;
			ctx512->count[1] = 0;
			ctx512->ops = sha512_get_ops();
			break;
		case SHA512_224_MECH_INFO_TYPE:
			ctx512->state[0] = 0x8c3d37c819544da2ULL;
			ctx512->state[1] = 0x73e1996689dcd4d6ULL;
			ctx512->state[2] = 0x1dfab7ae32ff9c82ULL;
			ctx512->state[3] = 0x679dd514582f9fcfULL;
			ctx512->state[4] = 0x0f6d2b697bd44da8ULL;
			ctx512->state[5] = 0x77e36f7304c48942ULL;
			ctx512->state[6] = 0x3f9d85a86a1d36c8ULL;
			ctx512->state[7] = 0x1112e6ad91d692a1ULL;
			ctx512->count[0] = 0;
			ctx512->count[1] = 0;
			ctx512->ops = sha512_get_ops();
			break;
		case SHA512_256_MECH_INFO_TYPE:
			ctx512->state[0] = 0x22312194fc2bf72cULL;
			ctx512->state[1] = 0x9f555fa3c84c64c2ULL;
			ctx512->state[2] = 0x2393b86b6f53b151ULL;
			ctx512->state[3] = 0x963877195940eabdULL;
			ctx512->state[4] = 0x96283ee2a88effe3ULL;
			ctx512->state[5] = 0xbe5e1e2553863992ULL;
			ctx512->state[6] = 0x2b0199fc2c85b8aaULL;
			ctx512->state[7] = 0x0eb72ddc81c52ca2ULL;
			ctx512->count[0] = 0;
			ctx512->count[1] = 0;
			ctx512->ops = sha512_get_ops();
			break;
	}
}

#define	IMPL_NAME		"sha256"
#define	IMPL_OPS_T		sha256_ops_t
#define	IMPL_ARRAY		sha256_impls
#define	IMPL_GET_OPS		sha256_get_ops
#define	ZFS_IMPL_OPS		zfs_sha256_ops

#define	IMPL_NAME		"sha512"
#define	IMPL_OPS_T		sha512_ops_t
#define	IMPL_ARRAY		sha512_impls
#define	IMPL_GET_OPS		sha512_get_ops
#define	ZFS_IMPL_OPS		zfs_sha512_ops

const IMPL_OPS_T *
IMPL_GET_OPS(void)
{
	const IMPL_OPS_T *ops = NULL;
	uint32_t idx, impl = IMPL_READ(generic_impl_chosen);
	static uint32_t cycle_count = 0;

	generic_impl_init();
	switch (impl) {
	case IMPL_FASTEST:
		ops = &generic_fastest_impl;
		break;
	case IMPL_CYCLE:
		idx = (++cycle_count) % generic_supp_impls_cnt;
		ops = generic_supp_impls[idx];
		break;
	default:
		ASSERT3U(impl, <, generic_supp_impls_cnt);
		ops = generic_supp_impls[impl];
		break;
	}

	ASSERT3P(ops, !=, NULL);
	return (ops);
}

/* Implementation that contains the fastest method */
static IMPL_OPS_T generic_fastest_impl = {
	.name = "fastest"
};

static void
generic_impl_init(void)
{
	int i, c;

	/* init only once */
	if (likely(generic_supp_impls_cnt != 0))
		return;

	/* Move supported implementations into generic_supp_impls */
	for (i = 0, c = 0; i < ARRAY_SIZE(IMPL_ARRAY); i++) {
		const IMPL_OPS_T *impl = IMPL_ARRAY[i];

		if (impl->is_supported && impl->is_supported())
			generic_supp_impls[c++] = impl;
	}
	generic_supp_impls_cnt = c;

	/* first init generic impl, may be changed via set_fastest() */
	memcpy(&generic_fastest_impl, generic_supp_impls[0],
	    sizeof (generic_fastest_impl));
}

static void
generic_impl_set_fastest(uint32_t id)
{
	generic_impl_init();
	memcpy(&generic_fastest_impl, generic_supp_impls[id],
	    sizeof (generic_fastest_impl));
}

static const IMPL_OPS_T *generic_supp_impls[ARRAY_SIZE(IMPL_ARRAY)];

static void
generic_impl_init(void)
{
	int i, c;

	/* init only once */
	if (likely(generic_supp_impls_cnt != 0))
		return;

	/* Move supported implementations into generic_supp_impls */
	for (i = 0, c = 0; i < ARRAY_SIZE(IMPL_ARRAY); i++) {
		const IMPL_OPS_T *impl = IMPL_ARRAY[i];

		if (impl->is_supported && impl->is_supported())
			generic_supp_impls[c++] = impl;
	}
	generic_supp_impls_cnt = c;

	/* first init generic impl, may be changed via set_fastest() */
	memcpy(&generic_fastest_impl, generic_supp_impls[0],
	    sizeof (generic_fastest_impl));
}

static const sha256_ops_t *const sha256_impls[] = {
	&sha256_generic_impl,
#if defined(__x86_64)
	&sha256_x64_impl,
#endif
#if defined(__x86_64) && defined(HAVE_SSSE3)
	&sha256_ssse3_impl,
#endif
#if defined(__x86_64) && defined(HAVE_AVX)
	&sha256_avx_impl,
#endif
#if defined(__x86_64) && defined(HAVE_AVX2)
	&sha256_avx2_impl,
#endif
#if defined(__x86_64) && defined(HAVE_SSE4_1)
	&sha256_shani_impl,
#endif
#if defined(__aarch64__) || (defined(__arm__) && __ARM_ARCH > 6)
	&sha256_armv7_impl,
	&sha256_neon_impl,
	&sha256_armv8_impl,
#endif
#if defined(__PPC64__)
	&sha256_ppc_impl,
	&sha256_power8_impl,
#endif /* __PPC64__ */
};

const sha256_ops_t sha256_generic_impl = {
	.name = "generic",
	.transform = sha256_generic,
	.is_supported = sha2_is_supported
};

const sha512_ops_t sha512_generic_impl = {
	.name = "generic",
	.transform = sha512_generic,
	.is_supported = sha2_is_supported
};

static const sha512_ops_t *const sha512_impls[] = {
	&sha512_generic_impl,
#if defined(__x86_64)
	&sha512_x64_impl,
#endif
#if defined(__x86_64) && defined(HAVE_AVX)
	&sha512_avx_impl,
#endif
#if defined(__x86_64) && defined(HAVE_AVX2)
	&sha512_avx2_impl,
#endif
#if defined(__aarch64__)
	&sha512_armv7_impl,
	&sha512_armv8_impl,
#endif
#if defined(__arm__) && __ARM_ARCH > 6
	&sha512_armv7_impl,
	&sha512_neon_impl,
#endif
#if defined(__PPC64__)
	&sha512_ppc_impl,
	&sha512_power8_impl,
#endif /* __PPC64__ */
};

const sha512_ops_t sha512_x64_impl = {
	.is_supported = sha2_is_supported,
	.transform = tf_sha512_transform_x64,
	.name = "x64"
};

const sha256_ops_t sha256_x64_impl = {
	.is_supported = sha2_is_supported,
	.transform = tf_sha256_transform_x64,
	.name = "x64"
};


/* Stub implementation for sha256_generic */
void sha256_generic(void) {}



/* Stub implementation for sha512_generic */
void sha512_generic(void) {}



/* Stub implementation for tf_sha512_transform_x64 */
void tf_sha512_transform_x64(void) {}



/* Stub implementation for tf_sha256_transform_x64 */
void tf_sha256_transform_x64(void) {}
