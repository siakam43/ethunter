/* CG-Bench fixture: fnptr-callback/example_14 */
/* fnptr: op, targets: relocate_ptrs */

void
gt_pch_p_14lang_tree_node (ATTRIBUTE_UNUSED void *this_obj,
	void *x_p,
	ATTRIBUTE_UNUSED gt_pointer_operator op,
	ATTRIBUTE_UNUSED void *cookie)
{
  union lang_tree_node * x ATTRIBUTE_UNUSED = (union lang_tree_node *)x_p;
  switch ((int) (lto_tree_node_structure (&((*x)))))
    {
    case TS_LTO_GENERIC:
      switch ((int) (tree_node_structure (&((*x).generic))))
        {
        case TS_BASE:
          break;
        case TS_TYPED:
          if ((void *)(x) == this_obj)
            op (&((*x).generic.typed.type), NULL, cookie);
          break;
        }
    }
}

void
gt_pch_nx_lang_tree_node (void *x_p)
{
  union lang_tree_node * x = (union lang_tree_node *)x_p;
  union lang_tree_node * xlimit = x;
  while (gt_pch_note_object (xlimit, xlimit, gt_pch_p_14lang_tree_node))
   xlimit = (CODE_CONTAINS_STRUCT (TREE_CODE (&(*xlimit).generic), TS_TYPE_COMMON) ? ((union lang_tree_node *) (*xlimit).generic.type_common.next_variant) : CODE_CONTAINS_STRUCT (TREE_CODE (&(*xlimit).generic), TS_COMMON) ? ((union lang_tree_node *) (*xlimit).generic.common.chain) : NULL);
}

int
gt_pch_note_object (void *obj, void *note_ptr_cookie,
		    gt_note_pointers note_ptr_fn,
		    size_t length_override)
{
  struct ptr_data **slot;

  if (obj == NULL || obj == (void *) 1)
    return 0;

  slot = (struct ptr_data **)
    saving_htab->find_slot_with_hash (obj, POINTER_HASH (obj), INSERT);
  if (*slot != NULL)
    {
      gcc_assert ((*slot)->note_ptr_fn == note_ptr_fn
		  && (*slot)->note_ptr_cookie == note_ptr_cookie);
      return 0;
    }

  *slot = XCNEW (struct ptr_data);
  (*slot)->obj = obj;
  (*slot)->note_ptr_fn = note_ptr_fn;
  (*slot)->note_ptr_cookie = note_ptr_cookie;
  if (length_override != (size_t)-1)
    (*slot)->size = length_override;
  else if (note_ptr_fn == gt_pch_p_S)
    (*slot)->size = strlen ((const char *)obj) + 1;
  else
    (*slot)->size = ggc_get_size (obj);
  return 1;
}

void
gt_pch_save (FILE *f)
{
    state.ptrs[i]->note_ptr_fn (state.ptrs[i]->obj,
                state.ptrs[i]->note_ptr_cookie,
                relocate_ptrs, &state);
}

static void
relocate_ptrs (void *ptr_p, void *real_ptr_p, void *state_p)
{
  void **ptr = (void **)ptr_p;
  struct traversal_state *state
    = (struct traversal_state *)state_p;
  struct ptr_data *result;

  if (*ptr == NULL || *ptr == (void *)1)
    return;

  result = (struct ptr_data *)
    saving_htab->find_with_hash (*ptr, POINTER_HASH (*ptr));
  gcc_assert (result);
  *ptr = result->new_addr;
  if (ptr_p == real_ptr_p)
    return;
  if (real_ptr_p == NULL)
    real_ptr_p = ptr_p;
  gcc_assert (real_ptr_p >= state->ptrs[state->ptrs_i]->obj
	      && ((char *) real_ptr_p + sizeof (void *)
		  <= ((char *) state->ptrs[state->ptrs_i]->obj
		      + state->ptrs[state->ptrs_i]->size)));
  void *addr
    = (void *) ((char *) state->ptrs[state->ptrs_i]->new_addr
		+ ((char *) real_ptr_p
		   - (char *) state->ptrs[state->ptrs_i]->obj));
  reloc_addrs_vec.safe_push (addr);
}