/* macro_fp cross-file: caller.c uses macros */
#define INVOKE(fn) fn()
#define INVOKE_PAIR(a, b) a(); b()

void macro_handler_a(void) {}
void macro_handler_b(void) {}

void caller_func(void) {
    INVOKE(macro_handler_a);
    INVOKE_PAIR(macro_handler_a, macro_handler_b);
}
