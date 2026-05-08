/* CG-Bench fixture: fnptr-global-array/example_1 */
/* fnptr: object_viewer[ZDB_OT_TYPE(doi.doi_bonus_type)], targets: dump_acl, dump_bpobj, dump_bpobj_subobjs, dump_ddt_zap, dump_dmu_objset, dump_dnode, dump_dsl_dataset, dump_dsl_dir, dump_history_offsets, dump_none, dump_packed_nvlist, dump_sa_attrs, dump_sa_layouts, dump_uint64, dump_uint8, dump_unknown, dump_zap, dump_znode, dump_zpldir */

static void
dump_object(objset_t *os, uint64_t object, int verbosity,
    boolean_t *print_header, uint64_t *dnode_slots_used, uint64_t flags)
{
	if (!dnode_held) {
		object_viewer[ZDB_OT_TYPE(doi.doi_bonus_type)](os,
				object, bonus, bsize);
	} else {
		(void) printf("\t\t(bonus encrypted)\n");
	}
}

static object_viewer_t *object_viewer[DMU_OT_NUMTYPES + 1] = {
	dump_none,		/* unallocated			*/
	dump_zap,		/* object directory		*/
	dump_uint64,		/* object array			*/
	dump_none,		/* packed nvlist		*/
	dump_packed_nvlist,	/* packed nvlist size		*/
	dump_none,		/* bpobj			*/
	dump_bpobj,		/* bpobj header			*/
	dump_none,		/* SPA space map header		*/
	dump_none,		/* SPA space map		*/
	dump_none,		/* ZIL intent log		*/
	dump_dnode,		/* DMU dnode			*/
	dump_dmu_objset,	/* DMU objset			*/
	dump_dsl_dir,		/* DSL directory		*/
	dump_zap,		/* DSL directory child map	*/
	dump_zap,		/* DSL dataset snap map		*/
	dump_zap,		/* DSL props			*/
	dump_dsl_dataset,	/* DSL dataset			*/
	dump_znode,		/* ZFS znode			*/
	dump_acl,		/* ZFS V0 ACL			*/
	dump_uint8,		/* ZFS plain file		*/
	dump_zpldir,		/* ZFS directory		*/
	dump_zap,		/* ZFS master node		*/
	dump_zap,		/* ZFS delete queue		*/
	dump_uint8,		/* zvol object			*/
	dump_zap,		/* zvol prop			*/
	dump_uint8,		/* other uint8[]		*/
	dump_uint64,		/* other uint64[]		*/
	dump_zap,		/* other ZAP			*/
	dump_zap,		/* persistent error log		*/
	dump_uint8,		/* SPA history			*/
	dump_history_offsets,	/* SPA history offsets		*/
	dump_zap,		/* Pool properties		*/
	dump_zap,		/* DSL permissions		*/
	dump_acl,		/* ZFS ACL			*/
	dump_uint8,		/* ZFS SYSACL			*/
	dump_none,		/* FUID nvlist			*/
	dump_packed_nvlist,	/* FUID nvlist size		*/
	dump_zap,		/* DSL dataset next clones	*/
	dump_zap,		/* DSL scrub queue		*/
	dump_zap,		/* ZFS user/group/project used	*/
	dump_zap,		/* ZFS user/group/project quota	*/
	dump_zap,		/* snapshot refcount tags	*/
	dump_ddt_zap,		/* DDT ZAP object		*/
	dump_zap,		/* DDT statistics		*/
	dump_znode,		/* SA object			*/
	dump_zap,		/* SA Master Node		*/
	dump_sa_attrs,		/* SA attribute registration	*/
	dump_sa_layouts,	/* SA attribute layouts		*/
	dump_zap,		/* DSL scrub translations	*/
	dump_none,		/* fake dedup BP		*/
	dump_zap,		/* deadlist			*/
	dump_none,		/* deadlist hdr			*/
	dump_zap,		/* dsl clones			*/
	dump_bpobj_subobjs,	/* bpobj subobjs		*/
	dump_unknown,		/* Unknown type, must be last	*/
};


/* Stub implementation for dump_acl */
void dump_acl(void) {}



/* Stub implementation for dump_bpobj */
void dump_bpobj(void) {}



/* Stub implementation for dump_bpobj_subobjs */
void dump_bpobj_subobjs(void) {}



/* Stub implementation for dump_ddt_zap */
void dump_ddt_zap(void) {}



/* Stub implementation for dump_dmu_objset */
void dump_dmu_objset(void) {}



/* Stub implementation for dump_dnode */
void dump_dnode(void) {}



/* Stub implementation for dump_dsl_dataset */
void dump_dsl_dataset(void) {}



/* Stub implementation for dump_dsl_dir */
void dump_dsl_dir(void) {}



/* Stub implementation for dump_history_offsets */
void dump_history_offsets(void) {}



/* Stub implementation for dump_none */
void dump_none(void) {}



/* Stub implementation for dump_packed_nvlist */
void dump_packed_nvlist(void) {}



/* Stub implementation for dump_sa_attrs */
void dump_sa_attrs(void) {}



/* Stub implementation for dump_sa_layouts */
void dump_sa_layouts(void) {}



/* Stub implementation for dump_uint64 */
void dump_uint64(void) {}



/* Stub implementation for dump_uint8 */
void dump_uint8(void) {}



/* Stub implementation for dump_unknown */
void dump_unknown(void) {}



/* Stub implementation for dump_zap */
void dump_zap(void) {}



/* Stub implementation for dump_znode */
void dump_znode(void) {}



/* Stub implementation for dump_zpldir */
void dump_zpldir(void) {}
