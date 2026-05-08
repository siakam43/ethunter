/* callback_param cross-file: callee.c defines the callback consumer */
typedef void (*cb_t)(int);

void my_callback(int x) {}

void execute(cb_t cb, int val) {
    cb(val);
}

void wrapper(void) {
    execute(my_callback, 1);
}
