/* CG-Bench fixture: fnptr-global-struct-array/example_1 */
/* fnptr: auxFieldHandlers[j].setter, targets: auxShardIdSetter, auxHumanNodenameSetter, auxTcpPortSetter, auxTlsPortSetter */

auxFieldHandler auxFieldHandlers[] = {
    {"shard-id", auxShardIdSetter, auxShardIdGetter, auxShardIdPresent},
    {"nodename", auxHumanNodenameSetter, auxHumanNodenameGetter, auxHumanNodenamePresent},
    {"tcp-port", auxTcpPortSetter, auxTcpPortGetter, auxTcpPortPresent},
    {"tls-port", auxTlsPortSetter, auxTlsPortGetter, auxTlsPortPresent},
};

for (unsigned j = 0; j < numElements(auxFieldHandlers); j++) {
    if (sdslen(field_argv[0]) != strlen(auxFieldHandlers[j].field) ||
        memcmp(field_argv[0], auxFieldHandlers[j].field, sdslen(field_argv[0])) != 0) {
        continue;
    }
    field_found = 1;
    aux_tcp_port |= j == af_tcp_port;
    aux_tls_port |= j == af_tls_port;
    if (auxFieldHandlers[j].setter(n, field_argv[1], sdslen(field_argv[1])) != C_OK) {
        /* Invalid aux field format */
        sdsfreesplitres(field_argv, field_argc);
        sdsfreesplitres(argv,argc);
        goto fmterr;
    }
}


/* Wrapper: calls through auxFieldHandlers[j].setter */
void setter_caller(void) {
    auxFieldHandlers[j].setter();
}



/* Stub implementation for auxShardIdSetter */
void auxShardIdSetter(void) {}



/* Stub implementation for auxHumanNodenameSetter */
void auxHumanNodenameSetter(void) {}



/* Stub implementation for auxTcpPortSetter */
void auxTcpPortSetter(void) {}



/* Stub implementation for auxTlsPortSetter */
void auxTlsPortSetter(void) {}
