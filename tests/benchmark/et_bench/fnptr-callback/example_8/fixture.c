/* ET-Bench fixture: fnptr-callback/example_8 */
/* Based on redis georadiusGeneric / sortCommandGeneric pattern */
/* fnptr: cmp, targets: sort_gp_asc, sort_gp_desc, sortCompare */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define SORT_NONE 0
#define SORT_ASC  1
#define SORT_DESC 2

typedef struct geoPoint {
    double score;
    double lat;
    double lon;
    char *member;
} geoPoint;

typedef struct geoArray {
    geoPoint *array;
    size_t used;
    size_t size;
} geoArray;

typedef struct client {
    int flags;
    void *reply;
} client;

typedef struct redisSortObject {
    void *obj;
    double score;
} redisSortObject;

typedef struct redisObject {
    int type;
    void *ptr;
} robj;

#define SWAPINIT(a, es) swaptype = ((size_t)a & (sizeof(long) - 1) || es & (sizeof(long) - 1)) ? 2 : es == sizeof(long) ? 0 : 1;

static inline void swap(char *a, char *b, int swaptype, size_t es) {
    char tmp;
    if (swaptype == 0) {
        long t = *(long *)a;
        *(long *)a = *(long *)b;
        *(long *)b = t;
    } else {
        for (size_t i = 0; i < es; i++) {
            tmp = a[i]; a[i] = b[i]; b[i] = tmp;
        }
    }
}

static inline char *
med3(char *a, char *b, char *c,
    int (*cmp) (const void *, const void *))
{
    return cmp(a, b) < 0 ?
           (cmp(b, c) < 0 ? b : (cmp(a, c) < 0 ? c : a ))
          :(cmp(b, c) > 0 ? b : (cmp(a, c) < 0 ? a : c ));
}

static void _pqsort(void *a, size_t n, size_t es,
    int (*cmp) (const void *, const void *), void *lrange, void *rrange)
{
    char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
    size_t d, r;
    int swaptype;

    SWAPINIT(a, es);
    if (n < 7) {
        for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
            for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
                 pl -= es)
                swap(pl, pl - es, swaptype, es);
        return;
    }
    pm = (char *) a + (n / 2) * es;
    if (n > 7) {
        pl = (char *) a;
        pn = (char *) a + (n - 1) * es;
        if (n > 40) {
            d = (n / 8) * es;
            pl = med3(pl, pl + d, pl + 2 * d, cmp);
            pm = med3(pm - d, pm, pm + d, cmp);
            pn = med3(pn - 2 * d, pn - d, pn, cmp);
        }
        pm = med3(pl, pm, pn, cmp);
    }
}

void
pqsort(void *a, size_t n, size_t es,
    int (*cmp) (const void *, const void *), size_t lrange, size_t rrange)
{
    _pqsort(a, n, es, cmp, ((unsigned char*)a) + (lrange * es),
                       ((unsigned char*)a) + ((rrange + 1) * es) - 1);
}

void georadiusGeneric(client *c, int srcKeyIndex, int flags) {
    robj *storekey = NULL;
    int sort = SORT_ASC;
    int returned_items = 10;
    int result_length = 20;
    geoArray ga_storage;
    geoArray *ga = &ga_storage;

    /* Process [optional] requested sorting */
    if (sort != SORT_NONE) {
        int (*sort_gp_callback)(const void *a, const void *b) = NULL;
        if (sort == SORT_ASC) {
            sort_gp_callback = sort_gp_asc;
        } else if (sort == SORT_DESC) {
            sort_gp_callback = sort_gp_desc;
        }

        if (returned_items == result_length) {
            qsort(ga->array, result_length, sizeof(geoPoint), sort_gp_callback);
        } else {
            pqsort(ga->array, result_length, sizeof(geoPoint), sort_gp_callback,
                0, (returned_items - 1));
        }
    }
}

void sortCommandGeneric(client *c, int readonly) {
    int dontsort = 0;
    robj *sortby = NULL;
    int start = 0;
    int end = 9;
    int vectorlen = 10;
    redisSortObject *vector = malloc(vectorlen * sizeof(redisSortObject));

    /* Now it's time to load the right scores in the sorting vector */
    if (!dontsort) {
        if (sortby && (start != 0 || end != vectorlen - 1))
            pqsort(vector, vectorlen, sizeof(redisSortObject), sortCompare, start, end);
        else
            qsort(vector, vectorlen, sizeof(redisSortObject), sortCompare);
    }
    free(vector);
}

int sort_gp_asc(const void *a, const void *b) {
    const geoPoint *ga = (const geoPoint *)a;
    const geoPoint *gb = (const geoPoint *)b;
    return (ga->score > gb->score) - (ga->score < gb->score);
}

int sort_gp_desc(const void *a, const void *b) {
    const geoPoint *ga = (const geoPoint *)a;
    const geoPoint *gb = (const geoPoint *)b;
    return (gb->score > ga->score) - (gb->score < ga->score);
}

int sortCompare(const void *a, const void *b) {
    const redisSortObject *sa = (const redisSortObject *)a;
    const redisSortObject *sb = (const redisSortObject *)b;
    if (sa->score > sb->score) return 1;
    if (sa->score < sb->score) return -1;
    return 0;
}
