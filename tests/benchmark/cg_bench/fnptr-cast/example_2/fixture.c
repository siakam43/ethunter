/* CG-Bench fixture: fnptr-cast/example_2 */
/* fnptr: ops->fmdo_close, targets: zfs_fm_close */

void fmd_case_close(fmd_hdl_t *hdl, fmd_case_t *cp)
{
	fmd_module_t *mp = (fmd_module_t *)hdl;
	const fmd_hdl_ops_t *ops = mp->mod_info->fmdi_ops;

	fmd_hdl_debug(hdl, "case closed (%s)", cp->ci_uuid);

	if (ops->fmdo_close != NULL)
		ops->fmdo_close(hdl, cp);

	mp->mod_stats.ms_caseopen.fmds_value.ui64--;
	mp->mod_stats.ms_caseclosed.fmds_value.ui64++;

	if (cp->ci_bufptr != NULL && cp->ci_bufsiz > 0)
		fmd_hdl_free(hdl, cp->ci_bufptr, cp->ci_bufsiz);

	fmd_hdl_free(hdl, cp, sizeof (fmd_case_t));
}

int fmd_hdl_register(fmd_hdl_t *hdl, int version, const fmd_hdl_info_t *mip)
{
	(void) version;
	fmd_module_t *mp = (fmd_module_t *)hdl;

	mp->mod_info = mip;
	mp->mod_name = mip->fmdi_desc + 4;	/* drop 'ZFS ' prefix */
	mp->mod_spec = NULL;

	/* bare minimum module stats */
	(void) strcpy(mp->mod_stats.ms_accepted.fmds_name, "fmd.accepted");
	(void) strcpy(mp->mod_stats.ms_caseopen.fmds_name, "fmd.caseopen");
	(void) strcpy(mp->mod_stats.ms_casesolved.fmds_name, "fmd.casesolved");
	(void) strcpy(mp->mod_stats.ms_caseclosed.fmds_name, "fmd.caseclosed");

	fmd_serd_hash_create(&mp->mod_serds);

	fmd_hdl_debug(hdl, "register module");

	return (0);
}

void _zfs_retire_init(fmd_hdl_t *hdl)
{
	zfs_retire_data_t *zdp;
	libzfs_handle_t *zhdl;

	if ((zhdl = libzfs_init()) == NULL)
		return;

	if (fmd_hdl_register(hdl, FMD_API_VERSION, &fmd_info) != 0) {
		libzfs_fini(zhdl);
		return;
	}

	zdp = fmd_hdl_zalloc(hdl, sizeof (zfs_retire_data_t), FMD_SLEEP);
	zdp->zrd_hdl = zhdl;

	fmd_hdl_setspecific(hdl, zdp);
}

static const fmd_hdl_info_t fmd_info = {
	"ZFS Diagnosis Engine", "1.0", &fmd_ops, fmd_props
};

static const fmd_hdl_ops_t fmd_ops = {
	zfs_fm_recv,	/* fmdo_recv */
	zfs_fm_timeout,	/* fmdo_timeout */
	zfs_fm_close,	/* fmdo_close */
	NULL,		/* fmdo_stats */
	zfs_fm_gc,	/* fmdo_gc */
};

void
fmd_hdl_debug(fmd_hdl_t *hdl, const char *format, ...)
{
	char message[256];
	va_list vargs;
	fmd_module_t *mp = (fmd_module_t *)hdl;

	va_start(vargs, format);
	(void) vsnprintf(message, sizeof (message), format, vargs);
	va_end(vargs);

	/* prefix message with module name */
	zed_log_msg(LOG_INFO, "%s: %s", mp->mod_name, message);
}

void
fmd_hdl_free(fmd_hdl_t *hdl, void *data, size_t size)
{
	(void) hdl;
	umem_free(data, size);
}

void
fmd_serd_hash_create(fmd_serd_hash_t *shp)
{
	shp->sh_hashlen = FMD_STR_BUCKETS;
	shp->sh_hash = calloc(shp->sh_hashlen, sizeof (void *));
	shp->sh_count = 0;

	if (shp->sh_hash == NULL) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}

}

_LIBZFS_H libzfs_handle_t *libzfs_init(void) {
	...
	return (hdl);
}

static void
zfs_fm_close(fmd_hdl_t *hdl, fmd_case_t *cs)
{
	zfs_case_t *zcp = fmd_case_getspecific(hdl, cs);

	if (zcp->zc_data.zc_serd_checksum[0] != '\0')
		fmd_serd_destroy(hdl, zcp->zc_data.zc_serd_checksum);
	if (zcp->zc_data.zc_serd_io[0] != '\0')
		fmd_serd_destroy(hdl, zcp->zc_data.zc_serd_io);
	if (zcp->zc_data.zc_has_remove_timer)
		fmd_timer_remove(hdl, zcp->zc_remove_timer);

	uu_list_remove(zfs_cases, zcp);
	uu_list_node_fini(zcp, &zcp->zc_node, zfs_case_pool);
	fmd_hdl_free(hdl, zcp, sizeof (zfs_case_t));
}

static void
zfs_fm_gc(fmd_hdl_t *hdl)
{
	zfs_purge_cases(hdl);
}

static void
zfs_fm_timeout(fmd_hdl_t *hdl, id_t id, void *data)
{
	zfs_case_t *zcp = data;

	if (id == zcp->zc_remove_timer)
		zfs_case_solve(hdl, zcp, "fault.fs.zfs.vdev.io");
}

static void
zfs_fm_recv(fmd_hdl_t *hdl, fmd_event_t *ep, nvlist_t *nvl, const char *class)
{
	...
}