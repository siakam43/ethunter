/* Test fixture 13: dlsym hardcoded string */
void plugin_init(void) {}
void plugin_cleanup(void) {}

int main(void) {
    void *handle = dlopen("libplugin.so", 1);
    void (*init_fn)(void) = dlsym(handle, "plugin_init");
    init_fn();
    return 0;
}
