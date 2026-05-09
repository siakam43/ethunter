/* CG-Bench fixture: fnptr-virtual/example_1 */
/* fnptr: get_state_map_by_name, targets: noop_region_model_context::get_state_map_by_name, region_model_context_decorator::get_state_map_by_name */

class region_model_context
{
 public:
  /* Hook for clients to store pending diagnostics.
     Return true if the diagnostic was stored, or false if it was deleted.  */
  virtual bool warn (std::unique_ptr<pending_diagnostic> d) = 0;

  /* Hook for clients to add a note to the last previously stored
     pending diagnostic.  */
  virtual void add_note (std::unique_ptr<pending_note> pn) = 0;

  /* Hook for clients to be notified when an SVAL that was reachable
     in a previous state is no longer live, so that clients can emit warnings
     about leaks.  */
  virtual void on_svalue_leak (const svalue *sval) = 0;

  /* Hook for clients to be notified when the set of explicitly live
     svalues changes, so that they can purge state relating to dead
     svalues.  */
  virtual void on_liveness_change (const svalue_set &live_svalues,
				   const region_model *model) = 0;

  virtual logger *get_logger () = 0;

  /* Hook for clients to be notified when the condition
     "LHS OP RHS" is added to the region model.
     This exists so that state machines can detect tests on edges,
     and use them to trigger sm-state transitions (e.g. transitions due
     to ptrs becoming known to be NULL or non-NULL, rather than just
     "unchecked") */
  virtual void on_condition (const svalue *lhs,
			     enum tree_code op,
			     const svalue *rhs) = 0;

  /* Hook for clients to be notified when the condition that
     SVAL is within RANGES is added to the region model.
     Similar to on_condition, but for use when handling switch statements.
     RANGES is non-empty.  */
  virtual void on_bounded_ranges (const svalue &sval,
				  const bounded_ranges &ranges) = 0;

  /* Hook for clients to be notified when a frame is popped from the stack.  */
  virtual void on_pop_frame (const frame_region *) = 0;

  /* Hooks for clients to be notified when an unknown change happens
     to SVAL (in response to a call to an unknown function).  */
  virtual void on_unknown_change (const svalue *sval, bool is_mutable) = 0;

  /* Hooks for clients to be notified when a phi node is handled,
     where RHS is the pertinent argument.  */
  virtual void on_phi (const gphi *phi, tree rhs) = 0;

  /* Hooks for clients to be notified when the region model doesn't
     know how to handle the tree code of T at LOC.  */
  virtual void on_unexpected_tree_code (tree t,
					const dump_location_t &loc) = 0;

  /* Hook for clients to be notified when a function_decl escapes.  */
  virtual void on_escaped_function (tree fndecl) = 0;

  virtual uncertainty_t *get_uncertainty () = 0;

  /* Hook for clients to purge state involving SVAL.  */
  virtual void purge_state_involving (const svalue *sval) = 0;

  /* Hook for clients to split state with a non-standard path.  */
  virtual void bifurcate (std::unique_ptr<custom_edge_info> info) = 0;

  /* Hook for clients to terminate the standard path.  */
  virtual void terminate_path () = 0;

  virtual const extrinsic_state *get_ext_state () const = 0;

  /* Hook for clients to access the a specific state machine in
     any underlying program_state.  */
  virtual bool
  get_state_map_by_name (const char *name,
			 sm_state_map **out_smap,
			 const state_machine **out_sm,
			 unsigned *out_sm_idx,
			 std::unique_ptr<sm_context> *out_sm_context) = 0;

  /* Precanned ways for clients to access specific state machines.  */
  bool get_fd_map (sm_state_map **out_smap,
		   const state_machine **out_sm,
		   unsigned *out_sm_idx,
		   std::unique_ptr<sm_context> *out_sm_context)
  {
    return get_state_map_by_name ("file-descriptor", out_smap, out_sm,
				  out_sm_idx, out_sm_context);
  }
  bool get_malloc_map (sm_state_map **out_smap,
		       const state_machine **out_sm,
		       unsigned *out_sm_idx)
  {
    return get_state_map_by_name ("malloc", out_smap, out_sm, out_sm_idx, NULL);
  }
  bool get_taint_map (sm_state_map **out_smap,
		      const state_machine **out_sm,
		      unsigned *out_sm_idx)
  {
    return get_state_map_by_name ("taint", out_smap, out_sm, out_sm_idx, NULL);
  }

  bool possibly_tainted_p (const svalue *sval);

  /* Get the current statement, if any.  */
  virtual const gimple *get_stmt () const = 0;
};

class noop_region_model_context : public region_model_context
{
public:
  bool warn (std::unique_ptr<pending_diagnostic>) override { return false; }
  void add_note (std::unique_ptr<pending_note>) override;
  void on_svalue_leak (const svalue *) override {}
  void on_liveness_change (const svalue_set &,
			   const region_model *) override {}
  logger *get_logger () override { return NULL; }
  void on_condition (const svalue *lhs ATTRIBUTE_UNUSED,
		     enum tree_code op ATTRIBUTE_UNUSED,
		     const svalue *rhs ATTRIBUTE_UNUSED) override
  {
  }
  void on_bounded_ranges (const svalue &,
			  const bounded_ranges &) override
  {
  }
  void on_pop_frame (const frame_region *) override {}
  void on_unknown_change (const svalue *sval ATTRIBUTE_UNUSED,
			  bool is_mutable ATTRIBUTE_UNUSED) override
  {
  }
  void on_phi (const gphi *phi ATTRIBUTE_UNUSED,
	       tree rhs ATTRIBUTE_UNUSED) override
  {
  }
  void on_unexpected_tree_code (tree, const dump_location_t &) override {}

  void on_escaped_function (tree) override {}

  uncertainty_t *get_uncertainty () override { return NULL; }

  void purge_state_involving (const svalue *sval ATTRIBUTE_UNUSED) override {}

  void bifurcate (std::unique_ptr<custom_edge_info> info) override;
  void terminate_path () override;

  const extrinsic_state *get_ext_state () const override { return NULL; }

  bool get_state_map_by_name (const char *,
			      sm_state_map **,
			      const state_machine **,
			      unsigned *,
			      std::unique_ptr<sm_context> *) override
  {
    return false;
  }

  const gimple *get_stmt () const override { return NULL; }
};

class region_model_context_decorator : public region_model_context
{
 public:
  bool warn (std::unique_ptr<pending_diagnostic> d) override
  {
    return m_inner->warn (std::move (d));
  }

  void add_note (std::unique_ptr<pending_note> pn) override
  {
    m_inner->add_note (std::move (pn));
  }

  void on_svalue_leak (const svalue *sval) override
  {
    m_inner->on_svalue_leak (sval);
  }

  void on_liveness_change (const svalue_set &live_svalues,
			   const region_model *model) override
  {
    m_inner->on_liveness_change (live_svalues, model);
  }

  logger *get_logger () override
  {
    return m_inner->get_logger ();
  }

  void on_condition (const svalue *lhs,
		     enum tree_code op,
		     const svalue *rhs) override
  {
    m_inner->on_condition (lhs, op, rhs);
  }

  void on_bounded_ranges (const svalue &sval,
			  const bounded_ranges &ranges) override
  {
    m_inner->on_bounded_ranges (sval, ranges);
  }

  void on_pop_frame (const frame_region *frame_reg) override
  {
    m_inner->on_pop_frame (frame_reg);
  }

  void on_unknown_change (const svalue *sval, bool is_mutable) override
  {
    m_inner->on_unknown_change (sval, is_mutable);
  }

  void on_phi (const gphi *phi, tree rhs) override
  {
    m_inner->on_phi (phi, rhs);
  }

  void on_unexpected_tree_code (tree t,
				const dump_location_t &loc) override
  {
    m_inner->on_unexpected_tree_code (t, loc);
  }

  void on_escaped_function (tree fndecl) override
  {
    m_inner->on_escaped_function (fndecl);
  }

  uncertainty_t *get_uncertainty () override
  {
    return m_inner->get_uncertainty ();
  }

  void purge_state_involving (const svalue *sval) override
  {
    m_inner->purge_state_involving (sval);
  }

  void bifurcate (std::unique_ptr<custom_edge_info> info) override
  {
    m_inner->bifurcate (std::move (info));
  }

  void terminate_path () override
  {
    m_inner->terminate_path ();
  }

  const extrinsic_state *get_ext_state () const override
  {
    return m_inner->get_ext_state ();
  }

  bool get_state_map_by_name (const char *name,
			      sm_state_map **out_smap,
			      const state_machine **out_sm,
			      unsigned *out_sm_idx,
			      std::unique_ptr<sm_context> *out_sm_context)
    override
  {
    return m_inner->get_state_map_by_name (name, out_smap, out_sm, out_sm_idx,
					   out_sm_context);
  }

  const gimple *get_stmt () const override
  {
    return m_inner->get_stmt ();
  }

protected:
  region_model_context_decorator (region_model_context *inner)
  : m_inner (inner)
  {
    gcc_assert (m_inner);
  }

  region_model_context *m_inner;
};





