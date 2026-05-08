/* fp_alias cross-file: caller.c creates alias chain */
extern void (*fp1)(void);

void (*fp2)(void);

void caller_func(void) {
    fp2 = fp1;
    fp2();
}
