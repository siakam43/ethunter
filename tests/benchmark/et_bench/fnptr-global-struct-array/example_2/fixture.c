/* ET-Bench fixture: fnptr-global-struct-array/example_2 */
/* fnptr: features_table[i].present, targets: https_proxy_present */
/* Pattern: global array of feature structs with optional present check function pointer */

#include <stdio.h>
#include <stdlib.h>

#define CURL_VERSION_ALTSVC 0x01000000
#define CURL_VERSION_HTTPS_PROXY 0x02000000

typedef struct curl_info_data curl_info_data;

struct curl_info_data {
    int features;
    const char *const *feature_names;
};

typedef int (*present_fn)(curl_info_data *info);

struct feat {
    const char *name;
    present_fn present;
    int bitmask;
};

#define NUM_FEATURES 3

static const char *feature_names_storage[64];
static curl_info_data version_info;

static int https_proxy_present(curl_info_data *info) {
    (void)info;
    return 1;
}

static const struct feat features_table[NUM_FEATURES] = {
    {"alt-svc",     NULL,                CURL_VERSION_ALTSVC},
    {"HTTPS-proxy", https_proxy_present, CURL_VERSION_HTTPS_PROXY},
    {NULL, NULL, 0}
};

void curl_version_info(void)
{
    int features = 0;
    size_t n = 0;
    int i;

    for (i = 0; i < NUM_FEATURES; i++) {
        const struct feat *p = &features_table[i];
        if (!p->name) break;
        if (!p->present || p->present(&version_info)) {
            features |= p->bitmask;
            feature_names_storage[n++] = p->name;
        }
    }
    feature_names_storage[n] = NULL;
    version_info.features = features;
    version_info.feature_names = feature_names_storage;
}
