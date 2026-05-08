/* CG-Bench fixture: fnptr-library/example_2 */
/* fnptr: g->allocf, targets: l_alloc, lj_alloc_f */

#define setmref(r, p)	((r).ptr64 = (uint64_t)(void *)(p))
#define mref(r, t)	((t *)(void *)(r).ptr64)
#define G(L)			(mref(L->glref, global_State))

static LJ_AINLINE void lj_mem_free(global_State *g, void *p, size_t osize) {
  g->gc.total -= (GCSize)osize;
  g->allocf(g->allocd, p, osize, 0);
}

#if LJ_64 && !LJ_GC64 && !(defined(LUAJIT_USE_VALGRIND) && defined(LUAJIT_USE_SYSMALLOC))
lua_State *lj_state_newstate(lua_Alloc allocf, void *allocd)
#else
LUA_API lua_State *lua_newstate(lua_Alloc allocf, void *allocd)
#endif
{
  lua_State *L;
  global_State *g;
  ...
#ifndef LUAJIT_USE_SYSMALLOC
  if (allocf == LJ_ALLOCF_INTERNAL) {
    allocd = lj_alloc_create(&prng);
    if (!allocd) return NULL;
    allocf = lj_alloc_f;
  }
#endif
  GG = (GG_State *)allocf(allocd, NULL, 0, sizeof(GG_State));
  L = &GG->L;
  g = &GG->g;
  setmref(L->glref, g);
  g->allocf = allocf;
  g->allocd = allocd;

#ifndef LUAJIT_USE_SYSMALLOC
  if (allocf == lj_alloc_f) {
    lj_alloc_setprng(allocd, &g->prng);
  }
#endif
  if (lj_vm_cpcall(L, NULL, NULL, cpluaopen) != 0) {
    close_state(L);
    return NULL;
  }
  return L;
}

static lua_State *luaL_newstate(void)
{
    lua_State *L = lua_newstate(l_alloc, NULL);
    if (L)
        lua_atpanic(L, &panic);
    return L;
}

static void close_state(lua_State *L)
{
  global_State *g = G(L);
  lj_buf_free(g, &g->tmpbuf);
#ifndef LUAJIT_USE_SYSMALLOC
  if (g->allocf == lj_alloc_f)
    lj_alloc_destroy(g->allocd);
  else
#endif
  g->allocf(g->allocd, G2GG(g), sizeof(GG_State), 0);
}

static LJ_AINLINE void lj_buf_free(global_State *g, SBuf *sb)
{
  lj_mem_free(g, sbufB(sb), sbufsz(sb));
}

void lj_alloc_destroy(void *msp)
{
  mstate ms = (mstate)msp;
  msegmentptr sp = &ms->seg;
  while (sp != 0) {
    char *base = sp->base;
    size_t size = sp->size;
    sp = sp->next;
    CALL_MUNMAP(base, size);
  }
}

LJ_ASMF int lj_vm_cpcall(lua_State *L, lua_CFunction func, void *ud,
			 lua_CPFunction cp);

void *lj_alloc_create(PRNGState *rs)
{
  size_t tsize = DEFAULT_GRANULARITY;
  char *tbase;
  INIT_MMAP();
  UNUSED(rs);
  tbase = (char *)(CALL_MMAP(rs, tsize));
  if (tbase != CMFAIL) {
    size_t msize = pad_request(sizeof(struct malloc_state));
    mchunkptr mn;
    mchunkptr msp = align_as_chunk(tbase);
    mstate m = (mstate)(chunk2mem(msp));
    memset(m, 0, msize);
    msp->head = (msize|PINUSE_BIT|CINUSE_BIT);
    m->seg.base = tbase;
    m->seg.size = tsize;
    m->release_checks = MAX_RELEASE_CHECK_RATE;
    init_bins(m);
    mn = next_chunk(mem2chunk(m));
    init_top(m, mn, (size_t)((tbase + tsize) - (char *)mn) - TOP_FOOT_SIZE);
    return m;
  }
  return NULL;
}


/* Stub implementation for l_alloc */
void l_alloc(void) {}



/* Stub implementation for lj_alloc_f */
void lj_alloc_f(void) {}
