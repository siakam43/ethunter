/* lazy_init cross-file: caller.c calls through lazy init fp */
extern void (*global_fp)(void);
void init_fp(int mode);

void caller_func(void) {
    init_fp(1);
    global_fp();
}
