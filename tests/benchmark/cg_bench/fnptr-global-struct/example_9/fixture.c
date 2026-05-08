/* CG-Bench fixture: fnptr-global-struct/example_9 */
/* fnptr: stream_read_tree, targets: lto_input_tree */

static void
input_ssa_names (class lto_input_block *ib, class data_in *data_in,
		 struct function *fn)
{
  unsigned int i, size;

  size = streamer_read_uhwi (ib);
  init_tree_ssa (fn, size);
  cfun->gimple_df->in_ssa_p = true;
  init_ssa_operands (fn);

  i = streamer_read_uhwi (ib);
  while (i)
    {
      tree ssa_name, name;
      bool is_default_def;

      /* Skip over the elements that had been freed.  */
      while (SSANAMES (fn)->length () < i)
	SSANAMES (fn)->quick_push (NULL_TREE);

      is_default_def = (streamer_read_uchar (ib) != 0);
      name = stream_read_tree (ib, data_in);
      ssa_name = make_ssa_name_fn (fn, name, NULL);

      if (is_default_def)
	{
	  set_ssa_default_def (cfun, SSA_NAME_VAR (ssa_name), ssa_name);
	  SSA_NAME_DEF_STMT (ssa_name) = gimple_build_nop ();
	}

      i = streamer_read_uhwi (ib);
    }
}

#define stream_read_tree(IB, DATA_IN) \
    streamer_hooks.read_tree (IB, DATA_IN)

void
lto_streamer_hooks_init (void)
{
  streamer_hooks_init ();
  streamer_hooks.write_tree = lto_output_tree;
  streamer_hooks.read_tree = lto_input_tree;
  streamer_hooks.input_location = lto_input_location;
  streamer_hooks.output_location = lto_output_location;
  streamer_hooks.output_location_and_block = lto_output_location_and_block;
}


/* Stub implementation for lto_input_tree */
void lto_input_tree(void) {}
