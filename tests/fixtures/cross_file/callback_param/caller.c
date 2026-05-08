/* callback_param cross-file: caller.c defines a callback and passes it */
typedef void (*cb_t)(int);

void execute(cb_t cb, int val);
void wrapper(void);

void local_handler(int x) {}

void main_func(void) {
    execute(local_handler, 42);
    wrapper();
}
