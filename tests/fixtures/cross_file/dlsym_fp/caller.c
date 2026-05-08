/* dlsym_fp cross-file: caller.c uses dlsym */
void plugin_func_a(void);
void plugin_func_b(void);

int main(void) {
    void *h = dlopen("libplugin.so", 1);
    void (*fn_a)(void) = dlsym(h, "plugin_func_a");
    void (*fn_b)(void) = dlsym(h, "plugin_func_b");
    fn_a();
    return 0;
}
