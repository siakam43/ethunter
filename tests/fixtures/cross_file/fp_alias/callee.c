/* fp_alias cross-file: callee.c defines the original fp */
void target_func(void) {}
void (*fp1)(void) = target_func;
