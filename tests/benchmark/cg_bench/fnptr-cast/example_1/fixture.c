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
extern nstime_update_t *const nstime_update;
nstime_update_t *const nstime_update = nstime_update_impl;

/* Stub implementation for nstime_update_impl */
void nstime_update_impl(void) {}
