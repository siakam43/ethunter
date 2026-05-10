/* ET-Bench fixture: fnptr-struct/example_4 */
/* Scenario: Redis ae event loop — timeProc callback dispatch.
   fnptr: te->timeProc
   target: record_rate
   caller: processTimeEvents */

#include <stddef.h>

#define RECORD_INTERVAL_MS 1000

typedef struct aeEventLoop aeEventLoop;
typedef struct aeTimeEvent aeTimeEvent;

typedef int (aeTimeProc)(aeEventLoop *eventLoop, long long id, void *clientData);
typedef void (aeEventFinalizerProc)(void *clientData);

struct aeTimeEvent {
    long long id;
    int when_sec;
    int when_ms;
    aeTimeProc *timeProc;
    aeEventFinalizerProc *finalizerProc;
    void *clientData;
    aeTimeEvent *next;
};

struct aeEventLoop {
    int stop;
    void (*beforesleep)(aeEventLoop *);
    aeTimeEvent *timeEventHead;
};

typedef struct thread {
    aeEventLoop *loop;
} thread;

static int aeGetTime(int *sec, int *ms) {
    *sec = 0; *ms = 0;
    return 0;
}

/* Target: record callback */
static int record_rate(aeEventLoop *eventLoop, long long id, void *clientData)
{
    thread *t = (thread *)clientData;
    (void)eventLoop; (void)id; (void)t;
    return RECORD_INTERVAL_MS;
}

/* Caller: invokes te->timeProc through the struct */
static int processTimeEvents(aeEventLoop *eventLoop) {
    int processed = 0;
    aeTimeEvent *te = eventLoop->timeEventHead;
    int now_sec, now_ms;
    long long id;

    while (te) {
        aeGetTime(&now_sec, &now_ms);
        if (now_sec > te->when_sec ||
            (now_sec == te->when_sec && now_ms >= te->when_ms))
        {
            id = te->id;
            int retval = te->timeProc(eventLoop, id, te->clientData);
            if (retval > 0) {
                processed++;
            }
            te = te->next;
            continue;
        }
        te = te->next;
    }
    return processed;
}

long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc)
{
    aeTimeEvent *te = eventLoop->timeEventHead;
    static long long next_id = 1;

    te = (aeTimeEvent *)0; /* simplified allocation */
    te->id = next_id++;
    te->timeProc = proc;
    te->clientData = clientData;
    te->finalizerProc = finalizerProc;
    te->next = eventLoop->timeEventHead;
    eventLoop->timeEventHead = te;

    aeGetTime(&te->when_sec, &te->when_ms);
    return te->id;
}

void aeMain(aeEventLoop *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        if (eventLoop->beforesleep != NULL)
            eventLoop->beforesleep(eventLoop);
        /* aeProcessEvents called here */
    }
}

int aeProcessEvents(aeEventLoop *eventLoop, int flags)
{
    int processed = 0;
    if (flags & 1) /* AE_TIME_EVENTS */
        processed += processTimeEvents(eventLoop);
    return processed;
}

void *thread_main(void *arg) {
    thread *t = (thread *)arg;
    aeEventLoop *loop = t->loop;
    aeCreateTimeEvent(loop, RECORD_INTERVAL_MS, record_rate, t, NULL);
    aeMain(loop);
    return NULL;
}
