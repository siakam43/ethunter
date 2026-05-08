/* CG-Bench fixture: fnptr-only/example_6 */
/* fnptr: junk_alloc_callback, targets: default_junk_alloc */

static void *
do_rallocx(void *ptr, size_t size, int flags, bool is_realloc) {
	...

	if (config_fill && unlikely(opt_junk_alloc) && usize > old_usize
	    && !zero) {
		size_t excess_len = usize - old_usize;
		void *excess_start = (void *)((uintptr_t)p + old_usize);
		junk_alloc_callback(excess_start, excess_len);
	}
}

void (*junk_alloc_callback)(void *ptr, size_t size) = &default_junk_alloc;


/* Stub implementation for default_junk_alloc */
void default_junk_alloc(void) {}
