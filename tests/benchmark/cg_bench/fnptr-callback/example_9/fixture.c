/* CG-Bench fixture: fnptr-callback/example_9 */
/* fnptr: reply_function, targets: addReplyCommandInfo, addReplyCommandDocs */

void addReplyCommandSubCommands(client *c, struct redisCommand *cmd, void (*reply_function)(client*, struct redisCommand*), int use_map) {
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
    while((de = dictNext(di)) != NULL) {
        struct redisCommand *sub = (struct redisCommand *)dictGetVal(de);
        if (use_map)
            addReplyBulkCBuffer(c, sub->fullname, sdslen(sub->fullname));
        reply_function(c, sub);
    }
    dictReleaseIterator(di);
}

void addReplyCommandInfo(client *c, struct redisCommand *cmd) {
    if (!cmd) {
        addReplyNull(c);
    } else {
        int firstkey = 0, lastkey = 0, keystep = 0;
        if (cmd->legacy_range_key_spec.begin_search_type != KSPEC_BS_INVALID) {
            firstkey = cmd->legacy_range_key_spec.bs.index.pos;
            lastkey = cmd->legacy_range_key_spec.fk.range.lastkey;
            if (lastkey >= 0)
                lastkey += firstkey;
            keystep = cmd->legacy_range_key_spec.fk.range.keystep;
        }

        addReplyArrayLen(c, 10);
        addReplyBulkCBuffer(c, cmd->fullname, sdslen(cmd->fullname));
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

void addReplyCommandDocs(client *c, struct redisCommand *cmd) {
    /* Count our reply len so we don't have to use deferred reply. */
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
