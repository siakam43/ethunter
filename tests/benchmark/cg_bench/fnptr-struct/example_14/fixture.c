/* CG-Bench fixture: fnptr-struct/example_14 */
/* fnptr: s->handshake_func, targets: ossl_statem_accept, ossl_statem_connect */

int ssl3_write_bytes(SSL *s, int type, const void *buf_, size_t len,
                     size_t *written)
{
    if (SSL_in_init(s) && !ossl_statem_get_in_handshake(s)
            && s->early_data_state != SSL_EARLY_DATA_UNAUTH_WRITING) {
        i = s->handshake_func(s);
        /* SSLfatal() already called */
        if (i < 0)
            return i;
        if (i == 0) {
            return -1;
        }
    }
}

WORK_STATE tls_finish_handshake(SSL *s, WORK_STATE wst, int clearbufs, int stop)
{
    void (*cb) (const SSL *ssl, int type, int val) = NULL;
    int cleanuphand = s->statem.cleanuphand;

    if (clearbufs) {
        if (!SSL_IS_DTLS(s)
#ifndef OPENSSL_NO_SCTP
            /*
             * RFC6083: SCTP provides a reliable and in-sequence transport service for DTLS
             * messages that require it. Therefore, DTLS procedures for retransmissions
             * MUST NOT be used.
             * Hence the init_buf can be cleared when DTLS over SCTP as transport is used.
             */
            || BIO_dgram_is_sctp(SSL_get_wbio(s))
#endif
            ) {
            /*
             * We don't do this in DTLS over UDP because we may still need the init_buf
             * in case there are any unexpected retransmits
             */
            BUF_MEM_free(s->init_buf);
            s->init_buf = NULL;
        }

        if (!ssl_free_wbio_buffer(s)) {
            SSLfatal(s, SSL_AD_INTERNAL_ERROR, SSL_F_TLS_FINISH_HANDSHAKE,
                     ERR_R_INTERNAL_ERROR);
            return WORK_ERROR;
        }
        s->init_num = 0;
    }

    if (SSL_IS_TLS13(s) && !s->server
            && s->post_handshake_auth == SSL_PHA_REQUESTED)
        s->post_handshake_auth = SSL_PHA_EXT_SENT;

    /*
     * Only set if there was a Finished message and this isn't after a TLSv1.3
     * post handshake exchange
     */
    if (cleanuphand) {
        /* skipped if we just sent a HelloRequest */
        s->renegotiate = 0;
        s->new_session = 0;
        s->statem.cleanuphand = 0;
        s->ext.ticket_expected = 0;

        ssl3_cleanup_key_block(s);

        if (s->server) {
            /*
             * In TLSv1.3 we update the cache as part of constructing the
             * NewSessionTicket
             */
            if (!SSL_IS_TLS13(s))
                ssl_update_cache(s, SSL_SESS_CACHE_SERVER);

            /* N.B. s->ctx may not equal s->session_ctx */
            tsan_counter(&s->ctx->stats.sess_accept_good);
            s->handshake_func = ossl_statem_accept;
        } else {
            if (SSL_IS_TLS13(s)) {
                /*
                 * We encourage applications to only use TLSv1.3 tickets once,
                 * so we remove this one from the cache.
                 */
                if ((s->session_ctx->session_cache_mode
                     & SSL_SESS_CACHE_CLIENT) != 0)
                    SSL_CTX_remove_session(s->session_ctx, s->session);
            } else {
                /*
                 * In TLSv1.3 we update the cache as part of processing the
                 * NewSessionTicket
                 */
                ssl_update_cache(s, SSL_SESS_CACHE_CLIENT);
            }
            if (s->hit)
                tsan_counter(&s->session_ctx->stats.sess_hit);

            s->handshake_func = ossl_statem_connect;
            tsan_counter(&s->session_ctx->stats.sess_connect_good);
        }

        if (SSL_IS_DTLS(s)) {
            /* done with handshaking */
            s->d1->handshake_read_seq = 0;
            s->d1->handshake_write_seq = 0;
            s->d1->next_handshake_write_seq = 0;
            dtls1_clear_received_buffer(s);
        }
    }

    if (s->info_callback != NULL)
        cb = s->info_callback;
    else if (s->ctx->info_callback != NULL)
        cb = s->ctx->info_callback;

    /* The callback may expect us to not be in init at handshake done */
    ossl_statem_set_in_init(s, 0);

    if (cb != NULL) {
        if (cleanuphand
                || !SSL_IS_TLS13(s)
                || SSL_IS_FIRST_HANDSHAKE(s))
            cb(s, SSL_CB_HANDSHAKE_DONE, 1);
    }

    if (!stop) {
        /* If we've got more work to do we go back into init */
        ossl_statem_set_in_init(s, 1);
        return WORK_FINISHED_CONTINUE;
    }

    return WORK_FINISHED_STOP;
}


/* Stub implementation for ossl_statem_accept */
void ossl_statem_accept(void) {}



/* Stub implementation for ossl_statem_connect */
void ossl_statem_connect(void) {}
