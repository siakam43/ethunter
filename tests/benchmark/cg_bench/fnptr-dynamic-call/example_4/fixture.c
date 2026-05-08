/* CG-Bench fixture: fnptr-dynamic-call/example_4 */
/* fnptr: ctx->bind_engine, targets: bind_engine */

void engine_load_dynamic_int(void)
{
    ENGINE *toadd = engine_dynamic();
    if (!toadd)
        return;
    ENGINE_add(toadd);
    /*
     * If the "add" worked, it gets a structural reference. So either way, we
     * release our just-created reference.
     */
    ENGINE_free(toadd);
    /*
     * If the "add" didn't work, it was probably a conflict because it was
     * already added (eg. someone calling ENGINE_load_blah then calling
     * ENGINE_load_builtin_engines() perhaps).
     */
    ERR_clear_error();
}

static ENGINE *engine_dynamic(void)
{
    ENGINE *ret = ENGINE_new();
    if (ret == NULL)
        return NULL;
    if (!ENGINE_set_id(ret, engine_dynamic_id) ||
        !ENGINE_set_name(ret, engine_dynamic_name) ||
        !ENGINE_set_init_function(ret, dynamic_init) ||
        !ENGINE_set_finish_function(ret, dynamic_finish) ||
        !ENGINE_set_ctrl_function(ret, dynamic_ctrl) ||
        !ENGINE_set_flags(ret, ENGINE_FLAGS_BY_ID_COPY) ||
        !ENGINE_set_cmd_defns(ret, dynamic_cmd_defns)) {
        ENGINE_free(ret);
        return NULL;
    }
    return ret;
}

/* Add another "ENGINE" type into the list. */
int ENGINE_add(ENGINE *e)
{
    int to_return = 1;
    if (e == NULL) {
        ENGINEerr(ENGINE_F_ENGINE_ADD, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if ((e->id == NULL) || (e->name == NULL)) {
        ENGINEerr(ENGINE_F_ENGINE_ADD, ENGINE_R_ID_OR_NAME_MISSING);
        return 0;
    }
    CRYPTO_THREAD_write_lock(global_engine_lock);
    if (!engine_list_add(e)) {
        ENGINEerr(ENGINE_F_ENGINE_ADD, ENGINE_R_INTERNAL_LIST_ERROR);
        to_return = 0;
    }
    CRYPTO_THREAD_unlock(global_engine_lock);
    return to_return;
}

ENGINE *ENGINE_by_id(const char *id)
{
    ENGINE *iterator;
    char *load_dir = NULL;
    if (id == NULL) {
        ENGINEerr(ENGINE_F_ENGINE_BY_ID, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (!RUN_ONCE(&engine_lock_init, do_engine_lock_init)) {
        ENGINEerr(ENGINE_F_ENGINE_BY_ID, ERR_R_MALLOC_FAILURE);
        return NULL;
    }

    CRYPTO_THREAD_write_lock(global_engine_lock);
    iterator = engine_list_head;
    while (iterator && (strcmp(id, iterator->id) != 0))
        iterator = iterator->next;
    if (iterator != NULL) {
        /*
         * We need to return a structural reference. If this is an ENGINE
         * type that returns copies, make a duplicate - otherwise increment
         * the existing ENGINE's reference count.
         */
        if (iterator->flags & ENGINE_FLAGS_BY_ID_COPY) {
            ENGINE *cp = ENGINE_new();
            if (cp == NULL)
                iterator = NULL;
            else {
                engine_cpy(cp, iterator);
                iterator = cp;
            }
        } else {
            iterator->struct_ref++;
            engine_ref_debug(iterator, 0, 1);
        }
    }
    CRYPTO_THREAD_unlock(global_engine_lock);
    if (iterator != NULL)
        return iterator;
    /*
     * Prevent infinite recursion if we're looking for the dynamic engine.
     */
    if (strcmp(id, "dynamic")) {
        if ((load_dir = ossl_safe_getenv("OPENSSL_ENGINES")) == NULL)
            load_dir = ENGINESDIR;
        iterator = ENGINE_by_id("dynamic");
        if (!iterator || !ENGINE_ctrl_cmd_string(iterator, "ID", id, 0) ||
            !ENGINE_ctrl_cmd_string(iterator, "DIR_LOAD", "2", 0) ||
            !ENGINE_ctrl_cmd_string(iterator, "DIR_ADD",
                                    load_dir, 0) ||
            !ENGINE_ctrl_cmd_string(iterator, "LIST_ADD", "1", 0) ||
            !ENGINE_ctrl_cmd_string(iterator, "LOAD", NULL, 0))
            goto notfound;
        return iterator;
    }
 notfound:
    ENGINE_free(iterator);
    ENGINEerr(ENGINE_F_ENGINE_BY_ID, ENGINE_R_NO_SUCH_ENGINE);
    ERR_add_error_data(2, "id=", id);
    return NULL;
    /* EEK! Experimental code ends */
}

int ENGINE_ctrl_cmd_string(ENGINE *e, const char *cmd_name, const char *arg,
                           int cmd_optional)
{
    int num, flags;
    long l;
    char *ptr;

    if (e == NULL || cmd_name == NULL) {
        ENGINEerr(ENGINE_F_ENGINE_CTRL_CMD_STRING, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    if (e->ctrl == NULL
        || (num = ENGINE_ctrl(e, ENGINE_CTRL_GET_CMD_FROM_NAME,
                              0, (void *)cmd_name, NULL)) <= 0) {
    }
}

int ENGINE_set_ctrl_function(ENGINE *e, ENGINE_CTRL_FUNC_PTR ctrl_f)
{
    e->ctrl = ctrl_f;
    return 1;
}

int ENGINE_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f) (void))
{
    int ctrl_exists, ref_exists;
    if (e == NULL) {
        ENGINEerr(ENGINE_F_ENGINE_CTRL, ERR_R_PASSED_NULL_PARAMETER);
        return 0;
    }
    CRYPTO_THREAD_write_lock(global_engine_lock);
    ref_exists = ((e->struct_ref > 0) ? 1 : 0);
    CRYPTO_THREAD_unlock(global_engine_lock);
    ctrl_exists = ((e->ctrl == NULL) ? 0 : 1);
    if (!ref_exists) {
        ENGINEerr(ENGINE_F_ENGINE_CTRL, ENGINE_R_NO_REFERENCE);
        return 0;
    }
    /*
     * Intercept any "root-level" commands before trying to hand them on to
     * ctrl() handlers.
     */
    switch (cmd) {
    case ENGINE_CTRL_HAS_CTRL_FUNCTION:
        return ctrl_exists;
    case ENGINE_CTRL_GET_FIRST_CMD_TYPE:
    case ENGINE_CTRL_GET_NEXT_CMD_TYPE:
    case ENGINE_CTRL_GET_CMD_FROM_NAME:
    case ENGINE_CTRL_GET_NAME_LEN_FROM_CMD:
    case ENGINE_CTRL_GET_NAME_FROM_CMD:
    case ENGINE_CTRL_GET_DESC_LEN_FROM_CMD:
    case ENGINE_CTRL_GET_DESC_FROM_CMD:
    case ENGINE_CTRL_GET_CMD_FLAGS:
        if (ctrl_exists && !(e->flags & ENGINE_FLAGS_MANUAL_CMD_CTRL))
            return int_ctrl_helper(e, cmd, i, p, f);
        if (!ctrl_exists) {
            ENGINEerr(ENGINE_F_ENGINE_CTRL, ENGINE_R_NO_CONTROL_FUNCTION);
            /*
             * For these cmd-related functions, failure is indicated by a -1
             * return value (because 0 is used as a valid return in some
             * places).
             */
            return -1;
        }
    default:
        break;
    }
    /* Anything else requires a ctrl() handler to exist. */
    if (!ctrl_exists) {
        ENGINEerr(ENGINE_F_ENGINE_CTRL, ENGINE_R_NO_CONTROL_FUNCTION);
        return 0;
    }
    return e->ctrl(e, cmd, i, p, f);
}

static int dynamic_ctrl(ENGINE *e, int cmd, long i, void *p, void (*f) (void))
{
    dynamic_data_ctx *ctx = dynamic_get_data_ctx(e);
    switch (cmd) {
    case DYNAMIC_CMD_LOAD:
        return dynamic_load(e, ctx);
    }
}

/*
 * This function retrieves the context structure from an ENGINE's "ex_data",
 * or if it doesn't exist yet, sets it up.
 */
static dynamic_data_ctx *dynamic_get_data_ctx(ENGINE *e)
{
    dynamic_data_ctx *ctx;
    if (dynamic_ex_data_idx < 0) {
        /*
         * Create and register the ENGINE ex_data, and associate our "free"
         * function with it to ensure any allocated contexts get freed when
         * an ENGINE goes underground.
         */
        int new_idx = ENGINE_get_ex_new_index(0, NULL, NULL, NULL,
                                              dynamic_data_ctx_free_func);
        if (new_idx == -1) {
            ENGINEerr(ENGINE_F_DYNAMIC_GET_DATA_CTX, ENGINE_R_NO_INDEX);
            return NULL;
        }
        CRYPTO_THREAD_write_lock(global_engine_lock);
        /* Avoid a race by checking again inside this lock */
        if (dynamic_ex_data_idx < 0) {
            /* Good, someone didn't beat us to it */
            dynamic_ex_data_idx = new_idx;
            new_idx = -1;
        }
        CRYPTO_THREAD_unlock(global_engine_lock);
        /*
         * In theory we could "give back" the index here if (new_idx>-1), but
         * it's not possible and wouldn't gain us much if it were.
         */
    }
    ctx = (dynamic_data_ctx *)ENGINE_get_ex_data(e, dynamic_ex_data_idx);
    /* Check if the context needs to be created */
    if ((ctx == NULL) && !dynamic_set_data_ctx(e, &ctx))
        /* "set_data" will set errors if necessary */
        return NULL;
    return ctx;
}

static int dynamic_set_data_ctx(ENGINE *e, dynamic_data_ctx **ctx)
{
    dynamic_data_ctx *c = OPENSSL_zalloc(sizeof(*c));
    int ret = 1;

    if (c == NULL) {
        ENGINEerr(ENGINE_F_DYNAMIC_SET_DATA_CTX, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    c->dirs = sk_OPENSSL_STRING_new_null();
    if (c->dirs == NULL) {
        ENGINEerr(ENGINE_F_DYNAMIC_SET_DATA_CTX, ERR_R_MALLOC_FAILURE);
        OPENSSL_free(c);
        return 0;
    }
    c->DYNAMIC_F1 = "v_check";
    c->DYNAMIC_F2 = "bind_engine";
    c->dir_load = 1;
    CRYPTO_THREAD_write_lock(global_engine_lock);
    if ((*ctx = (dynamic_data_ctx *)ENGINE_get_ex_data(e,
                                                       dynamic_ex_data_idx))
        == NULL) {
        /* Good, we're the first */
        ret = ENGINE_set_ex_data(e, dynamic_ex_data_idx, c);
        if (ret) {
            *ctx = c;
            c = NULL;
        }
    }
    CRYPTO_THREAD_unlock(global_engine_lock);
    /*
     * If we lost the race to set the context, c is non-NULL and *ctx is the
     * context of the thread that won.
     */
    if (c)
        sk_OPENSSL_STRING_free(c->dirs);
    OPENSSL_free(c);
    return ret;
}

static int dynamic_load(ENGINE *e, dynamic_data_ctx *ctx)
{
    ENGINE cpy;
    dynamic_fns fns;

    if (ctx->dynamic_dso == NULL)
        ctx->dynamic_dso = DSO_new();
    if (ctx->dynamic_dso == NULL)
        return 0;
    if (!ctx->DYNAMIC_LIBNAME) {
        if (!ctx->engine_id)
            return 0;
        DSO_ctrl(ctx->dynamic_dso, DSO_CTRL_SET_FLAGS,
                 DSO_FLAG_NAME_TRANSLATION_EXT_ONLY, NULL);
        ctx->DYNAMIC_LIBNAME =
            DSO_convert_filename(ctx->dynamic_dso, ctx->engine_id);
    }
    if (!int_load(ctx)) {
        ENGINEerr(ENGINE_F_DYNAMIC_LOAD, ENGINE_R_DSO_NOT_FOUND);
        DSO_free(ctx->dynamic_dso);
        ctx->dynamic_dso = NULL;
        return 0;
    }
    /* We have to find a bind function otherwise it'll always end badly */
    if (!
        (ctx->bind_engine =
         (dynamic_bind_engine) DSO_bind_func(ctx->dynamic_dso,
                                             ctx->DYNAMIC_F2))) {
        ctx->bind_engine = NULL;
        DSO_free(ctx->dynamic_dso);
        ctx->dynamic_dso = NULL;
        ENGINEerr(ENGINE_F_DYNAMIC_LOAD, ENGINE_R_DSO_FAILURE);
        return 0;
    }
    /* Do we perform version checking? */
    if (!ctx->no_vcheck) {
        unsigned long vcheck_res = 0;
        /*
         * Now we try to find a version checking function and decide how to
         * cope with failure if/when it fails.
         */
        ctx->v_check =
            (dynamic_v_check_fn) DSO_bind_func(ctx->dynamic_dso,
                                               ctx->DYNAMIC_F1);
        if (ctx->v_check)
            vcheck_res = ctx->v_check(OSSL_DYNAMIC_VERSION);
        /*
         * We fail if the version checker veto'd the load *or* if it is
         * deferring to us (by returning its version) and we think it is too
         * old.
         */
        if (vcheck_res < OSSL_DYNAMIC_OLDEST) {
            /* Fail */
            ctx->bind_engine = NULL;
            ctx->v_check = NULL;
            DSO_free(ctx->dynamic_dso);
            ctx->dynamic_dso = NULL;
            ENGINEerr(ENGINE_F_DYNAMIC_LOAD,
                      ENGINE_R_VERSION_INCOMPATIBILITY);
            return 0;
        }
    }
    /*
     * First binary copy the ENGINE structure so that we can roll back if the
     * hand-over fails
     */
    memcpy(&cpy, e, sizeof(ENGINE));
    /*
     * Provide the ERR, "ex_data", memory, and locking callbacks so the
     * loaded library uses our state rather than its own. FIXME: As noted in
     * engine.h, much of this would be simplified if each area of code
     * provided its own "summary" structure of all related callbacks. It
     * would also increase opaqueness.
     */
    fns.static_state = ENGINE_get_static_state();
    CRYPTO_get_mem_functions(&fns.mem_fns.malloc_fn, &fns.mem_fns.realloc_fn,
                             &fns.mem_fns.free_fn);
    /*
     * Now that we've loaded the dynamic engine, make sure no "dynamic"
     * ENGINE elements will show through.
     */
    engine_set_all_null(e);

    /* Try to bind the ENGINE onto our own ENGINE structure */
    if (!ctx->bind_engine(e, ctx->engine_id, &fns)) {
        ctx->bind_engine = NULL;
        ctx->v_check = NULL;
        DSO_free(ctx->dynamic_dso);
        ctx->dynamic_dso = NULL;
        ENGINEerr(ENGINE_F_DYNAMIC_LOAD, ENGINE_R_INIT_FAILED);
        /* Copy the original ENGINE structure back */
        memcpy(e, &cpy, sizeof(ENGINE));
        return 0;
    }
    /* Do we try to add this ENGINE to the internal list too? */
    if (ctx->list_add_value > 0) {
        if (!ENGINE_add(e)) {
            /* Do we tolerate this or fail? */
            if (ctx->list_add_value > 1) {
                /*
                 * Fail - NB: By this time, it's too late to rollback, and
                 * trying to do so allows the bind_engine() code to have
                 * created leaks. We just have to fail where we are, after
                 * the ENGINE has changed.
                 */
                ENGINEerr(ENGINE_F_DYNAMIC_LOAD,
                          ENGINE_R_CONFLICTING_ENGINE_ID);
                return 0;
            }
            /* Tolerate */
            ERR_clear_error();
        }
    }
    return 1;
}

static int dynamic_set_data_ctx(ENGINE *e, dynamic_data_ctx **ctx)
{
    dynamic_data_ctx *c = OPENSSL_zalloc(sizeof(*c));
    int ret = 1;

    if (c == NULL) {
        ENGINEerr(ENGINE_F_DYNAMIC_SET_DATA_CTX, ERR_R_MALLOC_FAILURE);
        return 0;
    }
    c->dirs = sk_OPENSSL_STRING_new_null();
    if (c->dirs == NULL) {
        ENGINEerr(ENGINE_F_DYNAMIC_SET_DATA_CTX, ERR_R_MALLOC_FAILURE);
        OPENSSL_free(c);
        return 0;
    }
    c->DYNAMIC_F1 = "v_check";
    c->DYNAMIC_F2 = "bind_engine";
    c->dir_load = 1;
    CRYPTO_THREAD_write_lock(global_engine_lock);
    if ((*ctx = (dynamic_data_ctx *)ENGINE_get_ex_data(e,
                                                       dynamic_ex_data_idx))
        == NULL) {
        /* Good, we're the first */
        ret = ENGINE_set_ex_data(e, dynamic_ex_data_idx, c);
        if (ret) {
            *ctx = c;
            c = NULL;
        }
    }
    CRYPTO_THREAD_unlock(global_engine_lock);
    /*
     * If we lost the race to set the context, c is non-NULL and *ctx is the
     * context of the thread that won.
     */
    if (c)
        sk_OPENSSL_STRING_free(c->dirs);
    OPENSSL_free(c);
    return ret;
}

DSO_FUNC_TYPE DSO_bind_func(DSO *dso, const char *symname)
{
    DSO_FUNC_TYPE ret = NULL;

    if ((dso == NULL) || (symname == NULL)) {
        DSOerr(DSO_F_DSO_BIND_FUNC, ERR_R_PASSED_NULL_PARAMETER);
        return NULL;
    }
    if (dso->meth->dso_bind_func == NULL) {
        DSOerr(DSO_F_DSO_BIND_FUNC, DSO_R_UNSUPPORTED);
        return NULL;
    }
    if ((ret = dso->meth->dso_bind_func(dso, symname)) == NULL) {
        DSOerr(DSO_F_DSO_BIND_FUNC, DSO_R_SYM_FAILURE);
        return NULL;
    }
    /* Success */
    return ret;
}

struct dso_meth_st {
    const char *name;
    /*
     * Loads a shared library, NB: new DSO_METHODs must ensure that a
     * successful load populates the loaded_filename field, and likewise a
     * successful unload OPENSSL_frees and NULLs it out.
     */
    int (*dso_load) (DSO *dso);
    /* Unloads a shared library */
    int (*dso_unload) (DSO *dso);
    /*
     * Binds a function - assumes a return type of DSO_FUNC_TYPE. This should
     * be cast to the real function prototype by the caller. Platforms that
     * don't have compatible representations for different prototypes (this
     * is possible within ANSI C) are highly unlikely to have shared
     * libraries at all, let alone a DSO_METHOD implemented for them.
     */
    DSO_FUNC_TYPE (*dso_bind_func) (DSO *dso, const char *symname);
    /*
     * The generic (yuck) "ctrl()" function. NB: Negative return values
     * (rather than zero) indicate errors.
     */
    long (*dso_ctrl) (DSO *dso, int cmd, long larg, void *parg);
    /*
     * The default DSO_METHOD-specific function for converting filenames to a
     * canonical native form.
     */
    DSO_NAME_CONVERTER_FUNC dso_name_converter;
    /*
     * The default DSO_METHOD-specific function for converting filenames to a
     * canonical native form.
     */
    DSO_MERGER_FUNC dso_merger;
    /* [De]Initialisation handlers. */
    int (*init) (DSO *dso);
    int (*finish) (DSO *dso);
    /* Return pathname of the module containing location */
    int (*pathbyaddr) (void *addr, char *path, int sz);
    /* Perform global symbol lookup, i.e. among *all* modules */
    void *(*globallookup) (const char *symname);
};


/* Stub implementation for bind_engine */
void bind_engine() {}
