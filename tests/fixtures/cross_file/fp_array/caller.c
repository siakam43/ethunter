/* fp_array cross-file: caller.c */
extern void (*table[])(void);

void caller_func(void) {
    table[0]();
    table[1]();
}
