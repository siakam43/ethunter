/* CG-Bench fixture: fnptr-only/example_1 */
/* fnptr: zmalloc_oom_handler, targets: zmalloc_default_oom */

/* Allocate memory or panic */
void *zmalloc(size_t size) {
    void *ptr = ztrymalloc_usable_internal(size, NULL);
    if (!ptr) zmalloc_oom_handler(size);
    return ptr;
}

static void (*zmalloc_oom_handler)(size_t) = zmalloc_default_oom;


/* Stub implementation for zmalloc_default_oom */
void zmalloc_default_oom(void) {}
