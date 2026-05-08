/* CG-Bench fixture: fnptr-callback/example_3 */
/* fnptr: cbk, targets: nfs_is_shared_cb, nfs_copy_entries_cb */

static nfs_process_exports(const char *exports, const char *mountpoint,
    boolean_t (*cbk)(void *userdata, char *line, boolean_t found_mountpoint),
    void *userdata)
{
	int error = SA_OK;
	boolean_t cont = B_TRUE;

	FILE *oldfp = fopen(exports, "re");
	if (oldfp != NULL) {
		...

		while (cont && getline(&buf, &buflen, oldfp) != -1) {
			if (buf[0] == '\n' || buf[0] == '#')
				continue;

			cont = cbk(userdata, buf,
			    (sep = strpbrk(buf, "\t \n")) != NULL &&
			    sep - buf == mplen &&
			    strncmp(buf, mp, mplen) == 0);
		... 
	    }

	    return (error);
    }
}

static nfs_copy_entries(FILE *newfp, const char *exports, const char *mountpoint)
{
	fputs(FILE_HEADER, newfp);

	int error = nfs_process_exports(
	    exports, mountpoint, nfs_copy_entries_cb, newfp);

	if (error == SA_OK && ferror(newfp) != 0)
		error = ferror(newfp);

	return (error);
}

boolean_t nfs_is_shared_impl(const char *exports, sa_share_impl_t impl_share)
{
	boolean_t found = B_FALSE;
	nfs_process_exports(exports, impl_share->sa_mountpoint,
	    nfs_is_shared_cb, &found);
	return (found);
}


/* Wrapper: calls through cbk */
void cbk_caller(void *userdata, char *line, boolean_t found_mountpoint) {
    cbk(userdata, line, found_mountpoint);
}



/* Stub implementation for nfs_is_shared_cb */
void nfs_is_shared_cb(void) {}



/* Stub implementation for nfs_copy_entries_cb */
void nfs_copy_entries_cb(void) {}
