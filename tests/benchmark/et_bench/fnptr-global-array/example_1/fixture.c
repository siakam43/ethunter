/*
 * et_bench fixture: fnptr-global-array/example_1
 *
 * Scenario: ZFS-style object dump dispatch table.
 * The global array object_viewer[ZDB_OT_NUMTYPES + 1] maps DMU object types
 * to their dump functions. dump_object() indexes into this array and calls
 * through the function pointer.
 */

#include <stdio.h>
#include <stdint.h>

/* --- Types --- */
typedef void (*object_viewer_t)(void *os, uint64_t object, void *bonus, uint64_t bsize);

/* --- Constants --- */
enum {
	DMU_OT_NONE = 0,
	DMU_OT_OBJECT_DIRECTORY,
	DMU_OT_OBJECT_ARRAY,
	DMU_OT_PACKED_NVLIST,
	DMU_OT_PACKED_NVLIST_SIZE,
	DMU_OT_BPOBJ,
	DMU_OT_BPOBJ_HDR,
	DMU_OT_SPACE_MAP_HDR,
	DMU_OT_SPACE_MAP,
	DMU_OT_INTENT_LOG,
	DMU_OT_DNODE,
	DMU_OT_OBJSET,
	DMU_OT_DSL_DIR,
	DMU_OT_DSL_DIR_CHILD_MAP,
	DMU_OT_DSL_DS_SNAP_MAP,
	DMU_OT_DSL_PROPS,
	DMU_OT_DSL_DATASET,
	DMU_OT_ZNODE,
	DMU_OT_OLDACL,
	DMU_OT_PLAIN_FILE,
	DMU_OT_ZPLDIR,
	DMU_OT_MASTER_NODE,
	DMU_OT_DELETE_QUEUE,
	DMU_OT_ZVOL,
	DMU_OT_ZVOL_PROP,
	DMU_OT_OTHER_UINT8,
	DMU_OT_OTHER_UINT64,
	DMU_OT_OTHER_ZAP,
	DMU_OT_ERRLOG,
	DMU_OT_SPA_HISTORY,
	DMU_OT_SPA_HISTORY_OFFSETS,
	DMU_OT_POOL_PROPS,
	DMU_OT_DSL_PERMS,
	DMU_OT_ACL,
	DMU_OT_SYSACL,
	DMU_OT_FUID,
	DMU_OT_FUID_SIZE,
	DMU_OT_NEXT_CLONES,
	DMU_OT_SCAN_QUEUE,
	DMU_OT_USERGROUP_USED,
	DMU_OT_USERGROUP_QUOTA,
	DMU_OT_SNAPREFCOUNT,
	DMU_OT_DDT_ZAP,
	DMU_OT_DDT_STATS,
	DMU_OT_SA,
	DMU_OT_SA_MASTER_NODE,
	DMU_OT_SA_ATTR_REGISTRATION,
	DMU_OT_SA_ATTR_LAYOUTS,
	DMU_OT_SCAN_TRANSLATION,
	DMU_OT_DDT_BP,
	DMU_OT_DEADLIST,
	DMU_OT_DEADLIST_HDR,
	DMU_OT_CLONES,
	DMU_OT_BPOBJ_SUBOBJ,
	DMU_OT_UNKNOWN,
	DMU_OT_NUMTYPES
};

/* --- Target functions (19 unique callees) --- */

static void dump_none(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_zap(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
	printf("ZAP object %llu\n", (unsigned long long)object);
}

static void dump_uint64(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) bonus; (void) bsize;
	printf("UINT64 object %llu\n", (unsigned long long)object);
}

static void dump_uint8(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) bonus; (void) bsize;
	printf("UINT8 object %llu\n", (unsigned long long)object);
}

static void dump_packed_nvlist(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_bpobj(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_bpobj_subobjs(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_dnode(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_dmu_objset(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_dsl_dir(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_dsl_dataset(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_znode(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_acl(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_zpldir(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_history_offsets(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_sa_attrs(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_sa_layouts(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_unknown(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

static void dump_ddt_zap(void *os, uint64_t object, void *bonus, uint64_t bsize)
{
	(void) os; (void) object; (void) bonus; (void) bsize;
}

/* --- Global function pointer array --- */

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

/* --- Caller: indexes into the array and calls through --- */

static void
dump_object(int object_type)
{
	void *os = NULL;
	uint64_t object = 0;
	void *bonus = NULL;
	uint64_t bsize = 0;

	if (object_type >= 0 && object_type <= DMU_OT_NUMTYPES) {
		object_viewer[object_type](os, object, bonus, bsize);
	}
}
