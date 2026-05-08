/* CG-Bench fixture: fnptr-cast/example_7 */
/* fnptr: ops->compute_native, targets: fletcher_4_scalar_native, fletcher_4_superscalar_native, fletcher_4_superscalar4_native */

static int
abd_fletcher_4_iter(void *data, size_t size, void *private)
{
	zio_abd_checksum_data_t *cdp = (zio_abd_checksum_data_t *)private;
	fletcher_4_ctx_t *ctx = cdp->acd_ctx;
	fletcher_4_ops_t *ops = (fletcher_4_ops_t *)cdp->acd_private;
	boolean_t native = cdp->acd_byteorder == ZIO_CHECKSUM_NATIVE;
	uint64_t asize = P2ALIGN(size, FLETCHER_MIN_SIMD_SIZE);

	ASSERT(IS_P2ALIGNED(size, sizeof (uint32_t)));

	if (asize > 0) {
		if (native)
			ops->compute_native(ctx, data, asize);
		else
			ops->compute_byteswap(ctx, data, asize);

		size -= asize;
		data = (char *)data + asize;
	}

	if (size > 0) {
		ASSERT3U(size, <, FLETCHER_MIN_SIMD_SIZE);
		/* At this point we have to switch to scalar impl */
		abd_fletcher_4_simd2scalar(native, data, size, cdp);
	}

	return (0);
}

zio_abd_checksum_func_t fletcher_4_abd_ops = {
	.acf_init = abd_fletcher_4_init,
	.acf_fini = abd_fletcher_4_fini,
	.acf_iter = abd_fletcher_4_iter
};

typedef const struct zio_abd_checksum_func {
	zio_abd_checksum_init_t *acf_init;
	zio_abd_checksum_fini_t *acf_fini;
	zio_abd_checksum_iter_t *acf_iter;
} zio_abd_checksum_func_t;

static inline void
abd_fletcher_4_impl(abd_t *abd, uint64_t size, zio_abd_checksum_data_t *acdp)
{
	fletcher_4_abd_ops.acf_init(acdp);
	abd_iterate_func(abd, 0, size, fletcher_4_abd_ops.acf_iter, acdp);
	fletcher_4_abd_ops.acf_fini(acdp);
}

static void
abd_fletcher_4_init(zio_abd_checksum_data_t *cdp)
{
	const fletcher_4_ops_t *ops = fletcher_4_impl_get();
	cdp->acd_private = (void *) ops;

	if (ops->uses_fpu == B_TRUE) {
		kfpu_begin();
	}
	if (cdp->acd_byteorder == ZIO_CHECKSUM_NATIVE)
		ops->init_native(cdp->acd_ctx);
	else
		ops->init_byteswap(cdp->acd_ctx);

}

typedef int abd_iter_func_t(void *buf, size_t len, void *priv);

static inline const fletcher_4_ops_t *
fletcher_4_impl_get(void)
{
	if (!kfpu_allowed())
		return (&fletcher_4_superscalar4_ops);

	const fletcher_4_ops_t *ops = NULL;
	uint32_t impl = IMPL_READ(fletcher_4_impl_chosen);

	switch (impl) {
	case IMPL_FASTEST:
		ASSERT(fletcher_4_initialized);
		ops = &fletcher_4_fastest_impl;
		break;
	case IMPL_CYCLE:
		/* Cycle through supported implementations */
		ASSERT(fletcher_4_initialized);
		ASSERT3U(fletcher_4_supp_impls_cnt, >, 0);
		static uint32_t cycle_count = 0;
		uint32_t idx = (++cycle_count) % fletcher_4_supp_impls_cnt;
		ops = fletcher_4_supp_impls[idx];
		break;
	default:
		ASSERT3U(fletcher_4_supp_impls_cnt, >, 0);
		ASSERT3U(impl, <, fletcher_4_supp_impls_cnt);
		ops = fletcher_4_supp_impls[impl];
		break;
	}

	ASSERT3P(ops, !=, NULL);

	return (ops);
}

static const fletcher_4_ops_t fletcher_4_scalar_ops = {
	.init_native = fletcher_4_scalar_init,
	.fini_native = fletcher_4_scalar_fini,
	.compute_native = fletcher_4_scalar_native,
	.init_byteswap = fletcher_4_scalar_init,
	.fini_byteswap = fletcher_4_scalar_fini,
	.compute_byteswap = fletcher_4_scalar_byteswap,
	.valid = fletcher_4_scalar_valid,
	.uses_fpu = B_FALSE,
	.name = "scalar"
};

static fletcher_4_ops_t fletcher_4_fastest_impl = {
	.name = "fastest",
	.valid = fletcher_4_scalar_valid
};

static const fletcher_4_ops_t *fletcher_4_impls[] = {
	&fletcher_4_scalar_ops,
	&fletcher_4_superscalar_ops,
	&fletcher_4_superscalar4_ops,
#if defined(HAVE_SSE2)
	&fletcher_4_sse2_ops,
#endif
#if defined(HAVE_SSE2) && defined(HAVE_SSSE3)
	&fletcher_4_ssse3_ops,
#endif
#if defined(HAVE_AVX) && defined(HAVE_AVX2)
	&fletcher_4_avx2_ops,
#endif
#if defined(__x86_64) && defined(HAVE_AVX512F)
	&fletcher_4_avx512f_ops,
#endif
#if defined(__x86_64) && defined(HAVE_AVX512BW)
	&fletcher_4_avx512bw_ops,
#endif
#if defined(__aarch64__) && !defined(__FreeBSD__)
	&fletcher_4_aarch64_neon_ops,
#endif
};

const fletcher_4_ops_t fletcher_4_superscalar_ops = {
	.init_native = fletcher_4_superscalar_init,
	.compute_native = fletcher_4_superscalar_native,
	.fini_native = fletcher_4_superscalar_fini,
	.init_byteswap = fletcher_4_superscalar_init,
	.compute_byteswap = fletcher_4_superscalar_byteswap,
	.fini_byteswap = fletcher_4_superscalar_fini,
	.valid = fletcher_4_superscalar_valid,
	.uses_fpu = B_FALSE,
	.name = "superscalar"
};

const fletcher_4_ops_t fletcher_4_superscalar4_ops = {
	.init_native = fletcher_4_superscalar4_init,
	.compute_native = fletcher_4_superscalar4_native,
	.fini_native = fletcher_4_superscalar4_fini,
	.init_byteswap = fletcher_4_superscalar4_init,
	.compute_byteswap = fletcher_4_superscalar4_byteswap,
	.fini_byteswap = fletcher_4_superscalar4_fini,
	.valid = fletcher_4_superscalar4_valid,
	.uses_fpu = B_FALSE,
	.name = "superscalar4"
};


/* Stub implementation for fletcher_4_scalar_native */
void fletcher_4_scalar_native(void) {}



/* Stub implementation for fletcher_4_superscalar_native */
void fletcher_4_superscalar_native(void) {}



/* Stub implementation for fletcher_4_superscalar4_native */
void fletcher_4_superscalar4_native(void) {}
