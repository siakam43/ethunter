/* CG-Bench fixture: fnptr-callback/example_8 */
/* fnptr: cmp, targets: sort_gp_asc, sort_gp_desc, sortCompare */

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
	int swaptype, cmp_result;

loop:	SWAPINIT(a, es);
	if (n < 7) {
		for (pm = (char *) a + es; pm < (char *) a + n * es; pm += es)
			for (pl = pm; pl > (char *) a && cmp(pl - es, pl) > 0;
			     pl -= es)
				swap(pl, pl - es);
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
    _pqsort(a,n,es,cmp,((unsigned char*)a)+(lrange*es),
                       ((unsigned char*)a)+((rrange+1)*es)-1);
}

void georadiusGeneric(client *c, int srcKeyIndex, int flags) {
    robj *storekey = NULL;

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

    /* Now it's time to load the right scores in the sorting vector */
    if (!dontsort) {
        if (sortby && (start != 0 || end != vectorlen-1))
            pqsort(vector,vectorlen,sizeof(redisSortObject),sortCompare, start,end);
        else
            qsort(vector,vectorlen,sizeof(redisSortObject),sortCompare);
    }
}

/* Stub implementation for sort_gp_asc */
void sort_gp_asc(void) {}



/* Stub implementation for sort_gp_desc */
void sort_gp_desc(void) {}



/* Stub implementation for sortCompare */
void sortCompare(void) {}
