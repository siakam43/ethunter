/* Test fixture: param_assign — parameter stored in struct member */
/* Tests: handler.cb = param  where param was passed as function argument */

typedef void (*callback_t)(int);

struct handler {
    callback_t cb;
};

void my_handler(int x) {}

void setup(struct handler *h, callback_t cb) {
    h->cb = cb;
}

int main(void) {
    struct handler h;
    setup(&h, my_handler);
    h.cb(42);
    return 0;
}
