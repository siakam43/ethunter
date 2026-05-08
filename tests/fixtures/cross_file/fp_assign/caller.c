/* fp_assign cross-file: caller.c calls through extern fp */
extern void (*get_fp)(void);

void caller_func(void) {
    get_fp();
}
