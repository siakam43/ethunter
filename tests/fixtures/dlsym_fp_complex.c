/* Complex dlsym_fp: multiple dlsym calls with different symbol strings */

void plugin_start(void) {}
void plugin_stop(void) {}
void plugin_config(int v) {}

int main(void) {
    void *h = dlopen("libplugin.so", 1);
    void (*start_fn)(void) = dlsym(h, "plugin_start");
    void (*stop_fn)(void) = dlsym(h, "plugin_stop");
    start_fn();
    return 0;
}
