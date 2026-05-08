/* fp_return cross-file: caller.c */
typedef void (*action_t)(void);

action_t get_handler(int type);

void caller_func(void) {
    get_handler(0)();
    get_handler(1)();
}
