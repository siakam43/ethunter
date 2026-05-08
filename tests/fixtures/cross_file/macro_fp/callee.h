/* macro_fp cross-file: callee.h defines macros */
#define INVOKE(fn) fn()
#define INVOKE_PAIR(a, b) a(); b()

void macro_handler_a(void);
void macro_handler_b(void);
