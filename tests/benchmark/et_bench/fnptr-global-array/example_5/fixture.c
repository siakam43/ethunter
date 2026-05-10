/*
 * et_bench fixture: fnptr-global-array/example_5
 *
 * Scenario: libcurl multi-state initialization dispatch. A global array
 * finit[] maps CURLMstate values to state initialization functions.
 * mstate() indexes into this array and calls through.
 */

#include <stdio.h>

/* --- Types --- */

typedef enum {
    CURLM_STATE_INIT = 0,
    CURLM_STATE_PENDING,
    CURLM_STATE_CONNECT,
    CURLM_STATE_RESOLVING,
    CURLM_STATE_CONNECTING,
    CURLM_STATE_TUNNELING,
    CURLM_STATE_PROTOCONNECT,
    CURLM_STATE_PROTOCONNECTING,
    CURLM_STATE_DO,
    CURLM_STATE_DOING,
    CURLM_STATE_DOING_MORE,
    CURLM_STATE_DID,
    CURLM_STATE_PERFORMING,
    CURLM_STATE_RATELIMITING,
    CURLM_STATE_DONE,
    CURLM_STATE_COMPLETED,
    CURLM_STATE_MSGSENT,
    MSTATE_LAST
} CURLMstate;

typedef void (*init_multistate_func)(void *data);

/* --- Target functions (3 unique callees) --- */

static void Curl_init_CONNECT(void *data)
{
    (void)data;
}

static void before_perform(void *data)
{
    (void)data;
}

static void init_completed(void *data)
{
    (void)data;
}

/* --- Simulated Curl_easy --- */

typedef struct Curl_easy {
    CURLMstate mstate;
} Curl_easy;

/* --- Caller: defines finit[] locally and calls through --- */

static void mstate(Curl_easy *data, CURLMstate state)
{
    static const init_multistate_func finit[MSTATE_LAST] = {
        NULL,              /* INIT */
        NULL,              /* PENDING */
        Curl_init_CONNECT, /* CONNECT */
        NULL,              /* RESOLVING */
        NULL,              /* CONNECTING */
        NULL,              /* TUNNELING */
        NULL,              /* PROTOCONNECT */
        NULL,              /* PROTOCONNECTING */
        NULL,              /* DO */
        NULL,              /* DOING */
        NULL,              /* DOING_MORE */
        before_perform,    /* DID */
        NULL,              /* PERFORMING */
        NULL,              /* RATELIMITING */
        NULL,              /* DONE */
        init_completed,    /* COMPLETED */
        NULL               /* MSGSENT */
    };

    CURLMstate oldstate = data->mstate;
    (void)oldstate;

    data->mstate = state;

    /* if this state has an init-function, run it */
    if (state < MSTATE_LAST && finit[state])
        finit[state](data);
}
