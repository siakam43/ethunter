/* Test fixture 12: Macro-generated function pointer operations */
#define CALL_HANDLER(h) h()

void handler_a(void) {}
void handler_b(void) {}

int main(void) {
    CALL_HANDLER(handler_a);
    return 0;
}
