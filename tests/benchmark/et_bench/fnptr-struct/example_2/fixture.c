/* ET-Bench fixture: fnptr-struct/example_2 */
/* Scenario: GCC cpp reader callback before_define.
   fnptr: pfile->cb.before_define
   target: dump_queued_macros
   caller: cpp_pop_definition */

#include <stddef.h>

struct def_pragma_macro {
    const char *name;
};

struct cpp_callbacks {
    void (*before_define)(struct cpp_reader *pfile);
    void (*used_define)(struct cpp_reader *pfile);
    void (*used_undef)(struct cpp_reader *pfile);
};

typedef struct cpp_reader cpp_reader;

struct cpp_reader {
    struct cpp_callbacks cb;
    void *line_map;
};

typedef struct cpp_reader *cpp_reader_ptr;

static void *
_cpp_lex_identifier(cpp_reader *pfile, const char *name)
{
    (void)pfile; (void)name;
    return NULL;
}

/* Target: dump_queued_macros */
static void dump_queued_macros(cpp_reader *pfile)
{
    if (pfile == NULL)
        return;
    /* Dump any queued macro definitions before processing */
}

/* Caller: invokes pfile->cb.before_define through the struct */
static void
cpp_pop_definition(cpp_reader *pfile, struct def_pragma_macro *c)
{
    void *node = _cpp_lex_identifier(pfile, c->name);
    if (node == NULL)
        return;

    if (pfile->cb.before_define)
        pfile->cb.before_define(pfile);

    /* Continue with popping the definition from the internal stack */
}

void
cpp_init_callbacks(cpp_reader *pfile)
{
    if (pfile == NULL)
        return;
    pfile->cb.before_define = dump_queued_macros;
    pfile->cb.used_define = NULL;
    pfile->cb.used_undef = NULL;
}
