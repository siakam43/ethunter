/* CG-Bench fixture: fnptr-struct/example_4 */
/* fnptr: te->timeProc, targets: record_rate */

/* Process time events */
static int processTimeEvents(aeEventLoop *eventLoop) {
    int processed = 0;
    ...
    while(te) {
        ...
        aeGetTime(&now_sec, &now_ms);
        if (now_sec > te->when_sec ||
            (now_sec == te->when_sec && now_ms >= te->when_ms))
        {
            ...
            retval = te->timeProc(eventLoop, id, te->clientData);
            ...
        }
        return processed;
    }
}

void *thread_main(void *arg) {
    thread *thread = arg;
    ...
    aeEventLoop *loop = thread->loop;
    aeCreateTimeEvent(loop, RECORD_INTERVAL_MS, record_rate, thread, NULL);

    ...
    aeMain(loop);
    ...
    return NULL;
}

long long aeCreateTimeEvent(aeEventLoop *eventLoop, long long milliseconds,
        aeTimeProc *proc, void *clientData,
        aeEventFinalizerProc *finalizerProc)
{
    ...
    te->timeProc = proc;
    ...
    return id;
}

void aeMain(aeEventLoop *eventLoop) {
    eventLoop->stop = 0;
    while (!eventLoop->stop) {
        if (eventLoop->beforesleep != NULL)
            eventLoop->beforesleep(eventLoop);
        aeProcessEvents(eventLoop, AE_ALL_EVENTS);
    }
}

int aeProcessEvents(aeEventLoop *eventLoop, int flags)
{
    ...
    /* Check time events */
    if (flags & AE_TIME_EVENTS)
        processed += processTimeEvents(eventLoop);

    return processed; /* return the number of processed file/time events */
}


/* Stub implementation for record_rate */
void record_rate(void) {}
