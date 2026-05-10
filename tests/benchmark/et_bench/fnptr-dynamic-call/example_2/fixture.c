/* ET-Bench fixture: fnptr-dynamic-call/example_2 */
/* fnptr: ret->sk_api_version, targets: sk_api_version */
/* Pattern: dlopen/dlsym, function pointer resolved at runtime then called */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

struct sshsk_provider {
    char *path;
    void *dlhandle;
    uint32_t (*sk_api_version)(void);
    int (*sk_enroll)(int);
    int (*sk_sign)(int);
};

void fatal(const char *fmt, ...);
void error(const char *fmt, ...);

static struct sshsk_provider *
sshsk_open(const char *path)
{
    struct sshsk_provider *ret = NULL;
    uint32_t version;

    if (path == NULL || *path == '\0') {
        error("No FIDO SecurityKeyProvider specified");
        return NULL;
    }

    ret = (struct sshsk_provider *)calloc(1, sizeof(*ret));
    if (ret == NULL) {
        error("calloc failed");
        return NULL;
    }

    ret->path = strdup(path);
    if (ret->path == NULL) {
        error("strdup failed");
        goto fail;
    }

    /* Load the shared library */
    if ((ret->dlhandle = dlopen(path, RTLD_NOW)) == NULL)
        fatal("Provider \"%s\" dlopen failed: %s", path, dlerror());

    /* Resolve the API version function */
    if ((ret->sk_api_version = dlsym(ret->dlhandle,
        "sk_api_version")) == NULL) {
        error("Provider \"%s\" dlsym(sk_api_version) failed: %s",
            path, dlerror());
        goto fail;
    }

    /* Call to verify compatibility */
    version = ret->sk_api_version();
    if (version < 1) {
        error("Provider API version %u is too old", version);
        goto fail;
    }

    return ret;

fail:
    if (ret->dlhandle)
        dlclose(ret->dlhandle);
    free(ret->path);
    free(ret);
    return NULL;
}

void fatal(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    exit(1);
}

void error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
}

/* Wrapper: calls through ret->sk_api_version */
uint32_t sk_api_version_caller(struct sshsk_provider *ret) {
    if (ret && ret->sk_api_version) {
        return ret->sk_api_version();
    }
    return 0;
}

/* Target: resolved via dlsym */
uint32_t sk_api_version(void) {
    return 2;
}
