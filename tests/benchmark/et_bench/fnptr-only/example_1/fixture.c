/* ET-Bench fixture: fnptr-only/example_1 */
/* fnptr: zmalloc_oom_handler, targets: zmalloc_default_oom */
/* Source: Redis-style memory allocator */

#include <stdlib.h>
#include <stdio.h>

static void zmalloc_default_oom(size_t size) {
    fprintf(stderr, "zmalloc: Out of memory trying to allocate %zu bytes\n", size);
    fflush(stderr);
    abort();
}

static void (*zmalloc_oom_handler)(size_t) = zmalloc_default_oom;

void *zmalloc(size_t size) {
    void *ptr = malloc(size + sizeof(size_t));
    if (!ptr) {
        zmalloc_oom_handler(size);
    }
    *((size_t *)ptr) = size;
    return (char *)ptr + sizeof(size_t);
}

void zfree(void *ptr) {
    if (ptr) {
        void *realptr = (char *)ptr - sizeof(size_t);
        free(realptr);
    }
}

void zmalloc_set_oom_handler(void (*handler)(size_t)) {
    zmalloc_oom_handler = handler;
}
