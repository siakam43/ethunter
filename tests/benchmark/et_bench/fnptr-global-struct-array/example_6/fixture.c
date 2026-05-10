/* ET-Bench fixture: fnptr-global-struct-array/example_6 */
/* fnptr: command_table[i].func, targets: zfs_do_version, zfs_do_create, zfs_do_destroy, zfs_do_snapshot, zfs_do_rollback, zfs_do_clone, zfs_do_promote, zfs_do_rename, zfs_do_bookmark, zfs_do_channel_program, zfs_do_list, zfs_do_set, zfs_do_get, zfs_do_inherit, zfs_do_upgrade, zfs_do_userspace, zfs_do_project, zfs_do_mount, zfs_do_unmount, zfs_do_share, zfs_do_unshare, zfs_do_send, zfs_do_receive, zfs_do_allow, zfs_do_receive, zfs_do_allow, zfs_do_unallow, zfs_do_hold, zfs_do_holds, zfs_do_release, zfs_do_diff, zfs_do_load_key, zfs_do_unload_key, zfs_do_change_key, zfs_do_redact, zfs_do_wait, zfs_do_zone, zfs_do_unzone */
/* Pattern: global command table with command handler function pointers, main loop dispatches */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HELP_VERSION 1
#define HELP_CREATE 2
#define HELP_DESTROY 3
#define HELP_SNAPSHOT 4
#define HELP_ROLLBACK 5
#define HELP_CLONE 6
#define HELP_PROMOTE 7
#define HELP_RENAME 8
#define HELP_BOOKMARK 9
#define HELP_CHANNEL_PROGRAM 10
#define HELP_LIST 11
#define HELP_SET 12
#define HELP_GET 13
#define HELP_INHERIT 14
#define HELP_UPGRADE 15
#define HELP_USERSPACE 16
#define HELP_PROJECTSPACE 17
#define HELP_PROJECT 18
#define HELP_MOUNT 19
#define HELP_UNMOUNT 20
#define HELP_SHARE 21
#define HELP_UNSHARE 22
#define HELP_SEND 23
#define HELP_RECEIVE 24
#define HELP_ALLOW 25
#define HELP_UNALLOW 26
#define HELP_HOLD 27
#define HELP_HOLDS 28
#define HELP_RELEASE 29
#define HELP_DIFF 30
#define HELP_LOAD_KEY 31
#define HELP_UNLOAD_KEY 32
#define HELP_CHANGE_KEY 33
#define HELP_REDACT 34
#define HELP_WAIT 35
#define HELP_ZONE 36
#define HELP_UNZONE 37

typedef struct zfs_command {
    const char *name;
    int (*func)(int argc, char **argv);
    int help;
} zfs_command_t;

/* All command handler targets */
static int zfs_do_version(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_create(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_destroy(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_snapshot(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_rollback(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_clone(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_promote(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_rename(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_bookmark(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_channel_program(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_list(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_set(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_get(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_inherit(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_upgrade(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_userspace(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_project(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_mount(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_unmount(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_share(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_unshare(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_send(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_receive(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_allow(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_unallow(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_hold(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_holds(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_release(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_diff(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_load_key(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_unload_key(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_change_key(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_redact(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_wait(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_zone(int argc, char **argv) { (void)argc; (void)argv; return 0; }
static int zfs_do_unzone(int argc, char **argv) { (void)argc; (void)argv; return 0; }

static zfs_command_t command_table[] = {
    { "version",   zfs_do_version,         HELP_VERSION },
    { "create",    zfs_do_create,          HELP_CREATE },
    { "destroy",   zfs_do_destroy,         HELP_DESTROY },
    { "snapshot",  zfs_do_snapshot,        HELP_SNAPSHOT },
    { "rollback",  zfs_do_rollback,        HELP_ROLLBACK },
    { "clone",     zfs_do_clone,           HELP_CLONE },
    { "promote",   zfs_do_promote,         HELP_PROMOTE },
    { "rename",    zfs_do_rename,          HELP_RENAME },
    { "bookmark",  zfs_do_bookmark,        HELP_BOOKMARK },
    { "program",   zfs_do_channel_program, HELP_CHANNEL_PROGRAM },
    { "list",      zfs_do_list,            HELP_LIST },
    { "set",       zfs_do_set,             HELP_SET },
    { "get",       zfs_do_get,             HELP_GET },
    { "inherit",   zfs_do_inherit,         HELP_INHERIT },
    { "upgrade",   zfs_do_upgrade,         HELP_UPGRADE },
    { "userspace", zfs_do_userspace,       HELP_USERSPACE },
    { "project",   zfs_do_project,         HELP_PROJECT },
    { "mount",     zfs_do_mount,           HELP_MOUNT },
    { "unmount",   zfs_do_unmount,         HELP_UNMOUNT },
    { "share",     zfs_do_share,           HELP_SHARE },
    { "unshare",   zfs_do_unshare,         HELP_UNSHARE },
    { "send",      zfs_do_send,            HELP_SEND },
    { "receive",   zfs_do_receive,         HELP_RECEIVE },
    { "allow",     zfs_do_allow,           HELP_ALLOW },
    { "unallow",   zfs_do_unallow,         HELP_UNALLOW },
    { "hold",      zfs_do_hold,            HELP_HOLD },
    { "holds",     zfs_do_holds,           HELP_HOLDS },
    { "release",   zfs_do_release,         HELP_RELEASE },
    { "diff",      zfs_do_diff,            HELP_DIFF },
    { "load-key",  zfs_do_load_key,        HELP_LOAD_KEY },
    { "unload-key", zfs_do_unload_key,     HELP_UNLOAD_KEY },
    { "change-key", zfs_do_change_key,     HELP_CHANGE_KEY },
    { "redact",    zfs_do_redact,          HELP_REDACT },
    { "wait",      zfs_do_wait,            HELP_WAIT },
    { "zone",      zfs_do_zone,            HELP_ZONE },
    { "unzone",    zfs_do_unzone,          HELP_UNZONE },
    { NULL, NULL, 0 }
};

static int find_command_idx(const char *name, unsigned *idx) {
    unsigned i;
    for (i = 0; command_table[i].name != NULL; i++) {
        if (strcmp(command_table[i].name, name) == 0) {
            *idx = i;
            return 1;
        }
    }
    return 0;
}

int main(int argc, char **argv)
{
    zfs_command_t *current_command;
    unsigned i;
    int ret = 0;
    char *cmdname;
    char **newargv;

    if (argc < 2) {
        fprintf(stderr, "Usage: zfs <command> ...\n");
        return 1;
    }

    cmdname = argv[1];
    newargv = argv + 1;

    if (find_command_idx(cmdname, &i)) {
        current_command = &command_table[i];
        ret = command_table[i].func(argc - 1, newargv + 1);
    } else {
        fprintf(stderr, "Unknown command: %s\n", cmdname);
        return 1;
    }
    return ret;
}
