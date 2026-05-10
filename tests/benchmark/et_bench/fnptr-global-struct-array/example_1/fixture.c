/* ET-Bench fixture: fnptr-global-struct-array/example_1 */
/* fnptr: auxFieldHandlers[j].setter, targets: auxShardIdSetter, auxHumanNodenameSetter, auxTcpPortSetter, auxTlsPortSetter */
/* Pattern: global array of structs with function pointer members, loop iterates and calls through */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef char *sds;
typedef struct clusterNode clusterNode;

typedef int fieldSetterFn(clusterNode *n, const char *value, size_t len);
typedef const char *fieldGetterFn(clusterNode *n);
typedef int fieldPresentFn(clusterNode *n);

#define C_OK 0

typedef struct {
    const char *field;
    fieldSetterFn *setter;
    fieldGetterFn *getter;
    fieldPresentFn *present;
} auxFieldHandler;

#define numElements(arr) (sizeof(arr) / sizeof(arr[0]))

/* Target functions */
static int auxShardIdSetter(clusterNode *n, const char *value, size_t len) {
    (void)n; (void)value; (void)len;
    return C_OK;
}
static int auxHumanNodenameSetter(clusterNode *n, const char *value, size_t len) {
    (void)n; (void)value; (void)len;
    return C_OK;
}
static int auxTcpPortSetter(clusterNode *n, const char *value, size_t len) {
    (void)n; (void)value; (void)len;
    return C_OK;
}
static int auxTlsPortSetter(clusterNode *n, const char *value, size_t len) {
    (void)n; (void)value; (void)len;
    return C_OK;
}

auxFieldHandler auxFieldHandlers[] = {
    {"shard-id", auxShardIdSetter, NULL, NULL},
    {"nodename", auxHumanNodenameSetter, NULL, NULL},
    {"tcp-port", auxTcpPortSetter, NULL, NULL},
    {"tls-port", auxTlsPortSetter, NULL, NULL},
};

void clusterProcessAuxField(clusterNode *n, const char *field_arg) {
    int field_found = 0;
    int aux_tcp_port = 0, aux_tls_port = 0;
    const char *field_argv[2] = { field_arg, "value" };
    unsigned j;

    for (j = 0; j < numElements(auxFieldHandlers); j++) {
        size_t field_len = strlen(field_argv[0]);
        if (strlen(auxFieldHandlers[j].field) != field_len ||
            memcmp(field_argv[0], auxFieldHandlers[j].field, field_len) != 0) {
            continue;
        }
        field_found = 1;
        aux_tcp_port |= (j == 2);
        aux_tls_port |= (j == 3);
        if (auxFieldHandlers[j].setter(n, field_argv[1], strlen(field_argv[1])) != C_OK) {
            fprintf(stderr, "Invalid aux field format\n");
            return;
        }
    }
    if (!field_found) {
        fprintf(stderr, "Unknown field: %s\n", field_arg);
    }
    (void)aux_tcp_port; (void)aux_tls_port;
}

/* Wrapper: calls through auxFieldHandlers[j].setter */
void setter_caller(clusterNode *n, unsigned j) {
    auxFieldHandlers[j].setter(n, "test", 4);
}
