/* CG-Bench fixture: fnptr-global-struct-array/example_2 */
/* fnptr: p->present, targets: https_proxy_present */

curl_version_info_data *curl_version_info(CURLversion stamp)
{
  size_t n;
  const struct feat *p;
  int features = 0;
  ...
  n = 0;
  for(p = features_table; p->name; p++)
    if(!p->present || p->present(&version_info)) {
      features |= p->bitmask;
      feature_names[n++] = p->name;
    }

  feature_names[n] = NULL;  /* Terminate array. */
  version_info.features = features;

  return &version_info;
}

#define FEATURE(name, present, bitmask) {(name), (present), (bitmask)}

struct feat {
  const char *name;
  int        (*present)(curl_version_info_data *info);
  int        bitmask;
};

static const struct feat features_table[] = {
#ifndef CURL_DISABLE_ALTSVC
  FEATURE("alt-svc",     NULL,                CURL_VERSION_ALTSVC),
#endif
...
#if defined(USE_SSL) && !defined(CURL_DISABLE_PROXY) && \
  !defined(CURL_DISABLE_HTTP)
  FEATURE("HTTPS-proxy", https_proxy_present, CURL_VERSION_HTTPS_PROXY),
#endif
...
}


/* Stub implementation for https_proxy_present */
void https_proxy_present(void) {}
