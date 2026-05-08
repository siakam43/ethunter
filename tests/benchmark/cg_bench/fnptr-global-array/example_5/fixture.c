/* CG-Bench fixture: fnptr-global-array/example_5 */
/* fnptr: finit[state], targets: Curl_init_CONNECT, before_perform, init_completed */

/* always use this function to change state, to make debugging easier */
static void mstate(struct Curl_easy *data, CURLMstate state
#ifdef DEBUGBUILD
                   , int lineno
#endif
)
{
  CURLMstate oldstate = data->mstate;
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
  ...
/* if this state has an init-function, run it */
  if(finit[state])
    finit[state](data);
}


/* Stub implementation for Curl_init_CONNECT */
void Curl_init_CONNECT(void) {}



/* Stub implementation for before_perform */
void before_perform(void) {}



/* Stub implementation for init_completed */
void init_completed(void) {}
