/* CG-Bench fixture: fnptr-struct/example_12 */
/* fnptr: s->ctx->ext.alpn_select_cb, targets: alpn_cb */

int tls_handle_alpn(SSL *s)
{
    if (s->ctx->ext.alpn_select_cb != NULL && s->s3->alpn_proposed != NULL) {
        int r = s->ctx->ext.alpn_select_cb(s, &selected, &selected_len,
                                           s->s3->alpn_proposed,
                                           (unsigned int)s->s3->alpn_proposed_len,
                                           s->ctx->ext.alpn_select_cb_arg);
    }
    return 1;
}

void SSL_CTX_set_alpn_select_cb(SSL_CTX *ctx,
                                SSL_CTX_alpn_select_cb_func cb,
                                void *arg)
{
    ctx->ext.alpn_select_cb = cb;
    ctx->ext.alpn_select_cb_arg = arg;
}

int s_server_main(int argc, char *argv[])
{
    if (alpn_ctx.data)
        SSL_CTX_set_alpn_select_cb(ctx, alpn_cb, &alpn_ctx);
}

static int alpn_cb(SSL *s, const unsigned char **out, unsigned char *outlen,
                   const unsigned char *in, unsigned int inlen, void *arg)
{
    tlsextalpnctx *alpn_ctx = arg;

    if (!s_quiet) {
        /* We can assume that |in| is syntactically valid. */
        unsigned int i;
        BIO_printf(bio_s_out, "ALPN protocols advertised by the client: ");
        for (i = 0; i < inlen;) {
            if (i)
                BIO_write(bio_s_out, ", ", 2);
            BIO_write(bio_s_out, &in[i + 1], in[i]);
            i += in[i] + 1;
        }
        BIO_write(bio_s_out, "\n", 1);
    }

    if (SSL_select_next_proto
        ((unsigned char **)out, outlen, alpn_ctx->data, alpn_ctx->len, in,
         inlen) != OPENSSL_NPN_NEGOTIATED) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    if (!s_quiet) {
        BIO_printf(bio_s_out, "ALPN protocols selected: ");
        BIO_write(bio_s_out, *out, *outlen);
        BIO_write(bio_s_out, "\n", 1);
    }

    return SSL_TLSEXT_ERR_OK;
}