/* CG-Bench fixture: fnptr-global-struct/example_3 */
/* fnptr: type.clientPubSubChannels, targets: getClientPubSubChannels */

/*
 * Pub/Sub type for global channels.
 */
pubsubtype pubSubType = {
    .shard = 0,
    .clientPubSubChannels = getClientPubSubChannels,
    .subscriptionCount = clientSubscriptionsCount,
    .serverPubSubChannels = &server.pubsub_channels,
    .subscribeMsg = &shared.subscribebulk,
    .unsubscribeMsg = &shared.unsubscribebulk,
    .messageBulk = &shared.messagebulk,
};

/* SUBSCRIBE channel [channel ...] */
void subscribeCommand(client *c) {
    int j;
    if ((c->flags & CLIENT_DENY_BLOCKING) && !(c->flags & CLIENT_MULTI)) {
        /**
         * A client that has CLIENT_DENY_BLOCKING flag on
         * expect a reply per command and so can not execute subscribe.
         *
         * Notice that we have a special treatment for multi because of
         * backward compatibility
         */
        addReplyError(c, "SUBSCRIBE isn't allowed for a DENY BLOCKING client");
        return;
    }
    for (j = 1; j < c->argc; j++)
        pubsubSubscribeChannel(c,c->argv[j],pubSubType);
    c->flags |= CLIENT_PUBSUB;
}

/* Subscribe a client to a channel. Returns 1 if the operation succeeded, or
 * 0 if the client was already subscribed to that channel. */
int pubsubSubscribeChannel(client *c, robj *channel, pubsubtype type) {
    dictEntry *de;
    list *clients = NULL;
    int retval = 0;

    /* Add the channel to the client -> channels hash table */
    if (dictAdd(type.clientPubSubChannels(c),channel,NULL) == DICT_OK) {
        retval = 1;
        incrRefCount(channel);
        /* Add the client to the channel -> list of clients hash table */
        de = dictFind(*type.serverPubSubChannels, channel);
        if (de == NULL) {
            clients = listCreate();
            dictAdd(*type.serverPubSubChannels, channel, clients);
            incrRefCount(channel);
        } else {
            clients = dictGetVal(de);
        }
        listAddNodeTail(clients,c);
    }
    /* Notify the client */
    addReplyPubsubSubscribed(c,channel,type);
    return retval;
}


/* Stub implementation for getClientPubSubChannels */
void getClientPubSubChannels(void) {}
