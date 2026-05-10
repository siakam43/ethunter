/* ET-Bench fixture: fnptr-callback/example_3 */
/* Based on Solaris sharefs nfs_process_exports pattern */
/* fnptr: cbk, targets: nfs_is_shared_cb, nfs_copy_entries_cb */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef int boolean_t;
#define B_TRUE  1
#define B_FALSE 0
#define SA_OK   0

typedef struct sa_share_impl {
    const char *sa_mountpoint;
} *sa_share_impl_t;

#define FILE_HEADER "# Export entries\n"

static int nfs_process_exports(const char *exports, const char *mountpoint,
    boolean_t (*cbk)(void *userdata, char *line, boolean_t found_mountpoint),
    void *userdata)
{
    int error = SA_OK;
    boolean_t cont = B_TRUE;
    char *buf = NULL;
    size_t buflen = 0;
    const char *mp = mountpoint;
    size_t mplen = mp ? strlen(mp) : 0;
    char *sep;

    FILE *oldfp = fopen(exports, "re");
    if (oldfp != NULL) {
        while (cont && getline(&buf, &buflen, oldfp) != -1) {
            if (buf[0] == '\n' || buf[0] == '#')
                continue;

            cont = cbk(userdata, buf,
                (sep = strpbrk(buf, "\t \n")) != NULL &&
                (long)(sep - buf) == (long)mplen &&
                strncmp(buf, mp, mplen) == 0);
        }

        fclose(oldfp);
        free(buf);
    }

    return error;
}

static int nfs_copy_entries(FILE *newfp, const char *exports, const char *mountpoint)
{
    fputs(FILE_HEADER, newfp);

    int error = nfs_process_exports(
        exports, mountpoint, nfs_copy_entries_cb, newfp);

    if (error == SA_OK && ferror(newfp) != 0)
        error = ferror(newfp);

    return error;
}

boolean_t nfs_is_shared_impl(const char *exports, sa_share_impl_t impl_share)
{
    boolean_t found = B_FALSE;
    nfs_process_exports(exports, impl_share->sa_mountpoint,
        nfs_is_shared_cb, &found);
    return found;
}

boolean_t nfs_is_shared_cb(void *userdata, char *line, boolean_t found_mountpoint) {
    boolean_t *found = (boolean_t *)userdata;
    if (found_mountpoint)
        *found = B_TRUE;
    return !found_mountpoint;
}

boolean_t nfs_copy_entries_cb(void *userdata, char *line, boolean_t found_mountpoint) {
    FILE *fp = (FILE *)userdata;
    fputs(line, fp);
    return B_TRUE;
}
