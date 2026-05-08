/* CG-Bench fixture: fnptr-cast/example_1 */
/* fnptr: nstime_update, targets: nstime_update_impl */

static void
hpa_hooks_curtime(nstime_t *r_nstime, bool first_reading) {
	if (first_reading) {
		nstime_init_zero(r_nstime);
	}
	nstime_update(r_nstime);
}

typedef void (nstime_update_t)(nstime_t *);
extern nstime_update_t *JET_MUTABLE nstime_update;
nstime_update_t *JET_MUTABLE nstime_update = nstime_update_impl;

/* Various function pointers are static and immutable except during testing. */
#ifdef JEMALLOC_JET
#  define JET_MUTABLE
#else
#  define JET_MUTABLE const
#endif


/* Stub implementation for nstime_update_impl */
void nstime_update_impl(void) {}
