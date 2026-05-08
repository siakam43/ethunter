/* CG-Bench fixture: fnptr-dynamic-call/example_2 */
/* fnptr: ret->sk_api_version, targets: sk_api_version */

if ((ret->dlhandle = dlopen(path, RTLD_NOW)) == NULL)
	fatal("Provider \"%s\" dlopen failed: %s", path, dlerror());
if ((ret->sk_api_version = dlsym(ret->dlhandle,
    "sk_api_version")) == NULL) {
	error("Provider \"%s\" dlsym(sk_api_version) failed: %s",
	    path, dlerror());
	goto fail;
}
version = ret->sk_api_version();


/* Wrapper: calls through ret->sk_api_version */
void sk_api_version_caller(void) {
    ret->sk_api_version();
}



/* Stub implementation for sk_api_version */
void sk_api_version(void) {}
