/* Test fixture 11: Lazy init of function pointers */
static void (*handler)(void) = (void *)0;

void default_handler(void) {}
void custom_handler(void) {}

void ensure_handler(void) {
    if (!handler) {
        handler = default_handler;
    }
}

int main(void) {
    ensure_handler();
    handler();
    return 0;
}
