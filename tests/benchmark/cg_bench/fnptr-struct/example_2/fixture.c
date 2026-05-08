/* CG-Bench fixture: fnptr-struct/example_2 */
/* fnptr: pfile->cb.before_define, targets: dump_queued_macros */

static void
cpp_pop_definition (cpp_reader *pfile, struct def_pragma_macro *c)
{
  cpp_hashnode *node = _cpp_lex_identifier (pfile, c->name);
  if (node == NULL)
    return;

  if (pfile->cb.before_define)
    pfile->cb.before_define (pfile);
  ...
}

if (flag_dump_macros == 'U')
  {
    cb->before_define = dump_queued_macros;
    cb->used_define = cb_used_define;
    cb->used_undef = cb_used_undef;
  }

if (gfc_cpp_option.dump_macros == 'U')
  {
    cb->before_define = dump_queued_macros;
  ...
   
  }


/* Stub implementation for dump_queued_macros */
void dump_queued_macros(void) {}
