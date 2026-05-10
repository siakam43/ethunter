/* ET-Bench fixture: fnptr-library/example_2 */
/* fnptr: g->allocf, targets: l_alloc, lj_alloc_f */
/* Pattern: library context with allocator function pointer, called through global state */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef void *(*lua_Alloc)(void *ud, void *ptr, size_t osize, size_t nsize);

#define LJ_64 1
#define LJ_GC64 0

typedef struct global_State {
    lua_Alloc allocf;
    void *allocd;
    size_t gc_total;
} global_State;

typedef struct lua_State {
    uint64_t glref;
} lua_State;

#define setmref(r, p)  ((r).ptr64 = (uint64_t)(void *)(p))
#define mref(r, t)     ((t *)(void *)(r).ptr64)
#define G(L)           (mref((L)->glref, global_State))

#define LJ_AINLINE static inline

static void lj_mem_free(global_State *g, void *p, size_t osize) {
    g->gc_total -= osize;
    g->allocf(g->allocd, p, osize, 0);
}

void *lj_alloc_f(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud;
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, nsize);
}

static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;
    if (nsize == 0) {
        free(ptr);
        return NULL;
    }
    return malloc(nsize);
}

lua_State *lua_newstate(lua_Alloc allocf, void *allocd);

static lua_State *luaL_newstate(void) {
    lua_State *L = lua_newstate(l_alloc, NULL);
    return L;
}

lua_State *lua_newstate(lua_Alloc allocf, void *allocd) {
    lua_State *L;
    global_State *g;

    L = (lua_State *)allocf(allocd, NULL, 0, sizeof(lua_State));
    if (!L) return NULL;
    g = (global_State *)allocf(allocd, NULL, 0, sizeof(global_State));
    if (!g) {
        allocf(allocd, L, sizeof(lua_State), 0);
        return NULL;
    }

    setmref(L->glref, g);
    g->allocf = allocf;
    g->allocd = allocd;
    g->gc_total = 0;

    return L;
}

static void close_state(lua_State *L) {
    global_State *g = G(L);
    lj_mem_free(g, g, sizeof(global_State));
}
