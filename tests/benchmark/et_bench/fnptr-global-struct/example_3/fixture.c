/* et_bench fixture: fnptr-global-struct/example_3 */
/* fnptr: type.clientPubSubChannels, targets: getClientPubSubChannels */

#include <stddef.h>

/* Minimal forward declarations */
typedef struct client client;
typedef struct robj robj;
typedef struct dictEntry dictEntry;
typedef struct list list;
typedef struct pubsubtype pubsubtype;
typedef struct serverPubSub serverPubSub;

/* Fake dict/list ops — just need names to compile */
#define DICT_OK 0
#define dictAdd(d, k, v) DICT_OK
#define dictFind(d, k) NULL
#define dictGetVal(e) NULL
#define listCreate() NULL
#define listAddNodeTail(l, c)
#define incrRefCount(o)

struct dictEntry { void *key; void *val; };

struct serverPubSub {
    void *channels;
};

/* The pubsubtype struct with a function pointer member */
struct pubsubtype {
    int shard;
    dictEntry *(*clientPubSubChannels)(client *c);
    int (*subscriptionCount)(client *c);
    struct serverPubSub **serverPubSubChannels;
    void **subscribeMsg;
    void **unsubscribeMsg;
    void **messageBulk;
};

struct client {
    int flags;
    int argc;
    robj **argv;
};

#define CLIENT_DENY_BLOCKING 1
#define CLIENT_MULTI 2
#define CLIENT_PUBSUB 4

struct robj { int type; };

/* Forward declarations */
void addReplyError(client *c, const char *msg);
void pubsubSubscribeChannel(client *c, robj *channel, pubsubtype type);
void addReplyPubsubSubscribed(client *c, robj *channel, pubsubtype type);

/* Global pubSubType struct with function pointer members */
dictEntry *getClientPubSubChannels(client *c);
int clientSubscriptionsCount(client *c);
struct serverPubSub *global_pubsub_channels;

struct pubsubtype pubSubType = {
    .shard = 0,
    .clientPubSubChannels = getClientPubSubChannels,
    .subscriptionCount = clientSubscriptionsCount,
    .serverPubSubChannels = &global_pubsub_channels,
    .subscribeMsg = NULL,
    .unsubscribeMsg = NULL,
    .messageBulk = NULL,
};

dictEntry *getClientPubSubChannels(client *c)
{
    return NULL;
}

int clientSubscriptionsCount(client *c)
{
    return 0;
}

void addReplyError(client *c, const char *msg) {}
void addReplyPubsubSubscribed(client *c, robj *channel, pubsubtype type) {}

/* Subscribe a client to a channel. */
void pubsubSubscribeChannel(client *c, robj *channel, pubsubtype type)
{
    dictEntry *de;
    list *clients = NULL;

    if (dictAdd(type.clientPubSubChannels(c), channel, NULL) == DICT_OK) {
        incrRefCount(channel);
        de = dictFind(*type.serverPubSubChannels, channel);
        if (de == NULL) {
            clients = listCreate();
        } else {
            clients = (list *)dictGetVal(de);
        }
        if (clients)
            listAddNodeTail(clients, c);
    }
    addReplyPubsubSubscribed(c, channel, type);
}

/* SUBSCRIBE channel [channel ...] */
void subscribeCommand(client *c)
{
    int j;
    if ((c->flags & CLIENT_DENY_BLOCKING) && !(c->flags & CLIENT_MULTI)) {
        addReplyError(c, "SUBSCRIBE isn't allowed for a DENY BLOCKING client");
        return;
    }
    for (j = 1; j < c->argc; j++)
        pubsubSubscribeChannel(c, c->argv[j], pubSubType);
    c->flags |= CLIENT_PUBSUB;
}
