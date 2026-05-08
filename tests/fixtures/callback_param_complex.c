/* Complex callback_param: multiple callback parameters, nested passing */

typedef void (*cb_t)(int);

void inner_handler(int x) {}
void outer_handler(int x) {}

void execute(cb_t cb, int val) {
    cb(val);
}

void wrapper(cb_t cb) {
    execute(inner_handler, 42);
    cb(99);
}

int main(void) {
    execute(inner_handler, 1);
    execute(outer_handler, 2);
    wrapper(inner_handler);
    return 0;
}
