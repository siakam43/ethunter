/* fp_assign cross-file: callee.c defines handler and fp variable */
void actual_handler(void) {}
void (*get_fp)(void) = actual_handler;
