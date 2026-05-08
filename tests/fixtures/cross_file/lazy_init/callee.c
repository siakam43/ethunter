/* lazy_init cross-file: callee.c defines the default handler */
void (*global_fp)(void) = (void *)0;

void default_handler(void) {}
void custom_handler(void) {}

void init_fp(int mode) {
    if (!global_fp) {
        if (mode) global_fp = custom_handler;
        else global_fp = default_handler;
    }
}
