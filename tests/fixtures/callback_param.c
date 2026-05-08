/* Test fixture 3: Callback as parameter */
typedef void (*callback_t)(int);

void do_callback(callback_t cb, int val) {
    cb(val);
}

void my_handler(int x) {
}

int main(void) {
    do_callback(my_handler, 42);
    return 0;
}
