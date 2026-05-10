/* et_bench fixture: fnptr-cast/example_1 */
/* fnptr: nstime_update, targets: nstime_update_impl */

#include <stdbool.h>

typedef struct {
    unsigned long ns;
} nstime_t;

static inline void nstime_init_zero(nstime_t *r) {
    r->ns = 0;
}

/* Function pointer type typedef — core of fnptr-cast pattern */
typedef void (nstime_update_t)(nstime_t *);

/* Extern declaration with explicit cast assignment */
extern nstime_update_t *const nstime_update;

/* The actual target function, cast-compatible with nstime_update_t */
void nstime_update_impl(nstime_t *r) {
    r->ns = 12345;
}

/* Explicit cast assignment: fnptr typed as typedef, assigned via cast */
nstime_update_t *const nstime_update = (nstime_update_t *)nstime_update_impl;

/* Real caller from jemalloc hpa_hooks_curtime */
static void
hpa_hooks_curtime(nstime_t *r_nstime, bool first_reading) {
    if (first_reading) {
        nstime_init_zero(r_nstime);
    }
    nstime_update(r_nstime);
}
