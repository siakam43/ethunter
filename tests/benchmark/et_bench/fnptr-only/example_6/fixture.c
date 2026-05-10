/* ET-Bench fixture: fnptr-only/example_6 */
/* fnptr: junk_alloc_callback, targets: default_junk_alloc */
/* Source: jemalloc-style memory sanitization */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define unlikely(x) __builtin_expect(!!(x), 0)

static int config_fill = 1;
static int opt_junk_alloc = 1;

static void default_junk_alloc(void *ptr, size_t size) {
    memset(ptr, 0xa5, size);
}

void (*junk_alloc_callback)(void *ptr, size_t size) = &default_junk_alloc;

static void *default_junk_free(void *ptr, size_t size) {
    memset(ptr, 0x5a, size);
    return ptr;
}

static void *
do_rallocx(void *ptr, size_t size, int flags, int is_realloc)
{
    if (!ptr)
        return malloc(size);

    size_t old_usize = 64;
    size_t usize = size;
    int zero = (flags & 1);

    void *p = realloc(ptr, size);
    if (!p)
        return NULL;

    if (config_fill && unlikely(opt_junk_alloc) && usize > old_usize
        && !zero) {
        size_t excess_len = usize - old_usize;
        void *excess_start = (void *)((uintptr_t)p + old_usize);
        junk_alloc_callback(excess_start, excess_len);
    }

    return p;
}

void set_junk_alloc_callback(void (*cb)(void *, size_t)) {
    junk_alloc_callback = cb;
}

void *rallocx(void *ptr, size_t size, int flags) {
    return do_rallocx(ptr, size, flags, 1);
}

void *mallocx(size_t size, int flags) {
    void *p = malloc(size);
    if (p && config_fill && unlikely(opt_junk_alloc)) {
        junk_alloc_callback(p, size);
    }
    return p;
}
