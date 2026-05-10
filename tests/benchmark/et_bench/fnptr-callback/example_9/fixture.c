/* ET-Bench fixture: fnptr-callback/example_9 */
/* Based on redis addReplyCommandSubCommands pattern */
/* fnptr: reply_function, targets: addReplyCommandInfo, addReplyCommandDocs */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct sds {
    char *buf;
    size_t len;
} *sds;

static size_t sdslen(sds s) { return s ? s->len : 0; }

typedef struct dictEntry {
    void *val;
    struct dictEntry *next;
} dictEntry;

typedef struct dict {
    size_t size;
    dictEntry *ht;
} dict;

typedef struct dictIterator {
    dictEntry *current;
    int index;
} dictIterator;

static size_t dictSize(dict *d) { return d ? d->size : 0; }
static dictIterator *dictGetSafeIterator(dict *d) { return calloc(1, sizeof(dictIterator)); }
static dictEntry *dictNext(dictIterator *di) { return di && di->current ? di->current : NULL; }
static void *dictGetVal(dictEntry *de) { return de ? de->val : NULL; }
static void dictReleaseIterator(dictIterator *di) { free(di); }

typedef struct redisCommand {
    const char *fullname;
    int arity;
    void *args;
    int num_args;
    dict *subcommands_dict;
    int legacy_range_key_spec_begin;
    int legacy_range_key_spec_lastkey;
    int legacy_range_key_spec_keystep;
} redisCommand;

typedef struct client {
    int flags;
    void *reply;
} client;

void addReplySetLen(client *c, size_t n) {}
void addReplyMapLen(client *c, size_t n) {}
void addReplyArrayLen(client *c, size_t n) {}
void addReplyBulkCBuffer(client *c, const char *s, size_t len) {}
void addReplyNull(client *c) {}
void addReplyLongLong(client *c, long long v) {}
void addReplyFlagsForCommand(client *c, redisCommand *cmd) {}
void addReplyCommandCategories(client *c, redisCommand *cmd) {}
void addReplyCommandTips(client *c, redisCommand *cmd) {}
void addReplyCommandKeySpecs(client *c, redisCommand *cmd) {}
void addReplyBulkCString(client *c, const char *s) {}
void addReplyCommandArgList(client *c, void *args, int n) {}

void addReplyCommandSubCommands(client *c, redisCommand *cmd,
    void (*reply_function)(client*, redisCommand*), int use_map) {
    if (!cmd->subcommands_dict) {
        addReplySetLen(c, 0);
        return;
    }

    if (use_map)
        addReplyMapLen(c, dictSize(cmd->subcommands_dict));
    else
        addReplyArrayLen(c, dictSize(cmd->subcommands_dict));
    dictEntry *de;
    dictIterator *di = dictGetSafeIterator(cmd->subcommands_dict);
    while ((de = dictNext(di)) != NULL) {
        redisCommand *sub = (redisCommand *)dictGetVal(de);
        if (use_map)
            addReplyBulkCBuffer(c, sub->fullname, sdslen((sds)sub->fullname));
        reply_function(c, sub);
    }
    dictReleaseIterator(di);
}

void addReplyCommandInfo(client *c, redisCommand *cmd) {
    if (!cmd) {
        addReplyNull(c);
    } else {
        int firstkey = 0, lastkey = 0, keystep = 0;
        if (cmd->legacy_range_key_spec_begin != 0) {
            firstkey = cmd->legacy_range_key_spec_begin;
            lastkey = cmd->legacy_range_key_spec_lastkey;
            if (lastkey >= 0)
                lastkey += firstkey;
            keystep = cmd->legacy_range_key_spec_keystep;
        }

        addReplyArrayLen(c, 10);
        addReplyBulkCBuffer(c, cmd->fullname, sdslen((sds)cmd->fullname));
        addReplyLongLong(c, cmd->arity);
        addReplyFlagsForCommand(c, cmd);
        addReplyLongLong(c, firstkey);
        addReplyLongLong(c, lastkey);
        addReplyLongLong(c, keystep);
        addReplyCommandCategories(c, cmd);
        addReplyCommandTips(c, cmd);
        addReplyCommandKeySpecs(c, cmd);
        addReplyCommandSubCommands(c, cmd, addReplyCommandInfo, 0);
    }
}

void addReplyCommandDocs(client *c, redisCommand *cmd) {
    long maplen = 1;
    int needargs = 1;
    if (cmd->args) {
        addReplyBulkCString(c, "arguments");
        addReplyCommandArgList(c, cmd->args, cmd->num_args);
    }
    if (cmd->subcommands_dict) {
        addReplyBulkCString(c, "subcommands");
        addReplyCommandSubCommands(c, cmd, addReplyCommandDocs, 1);
    }
}
