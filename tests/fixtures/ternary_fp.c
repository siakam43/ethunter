/* Edge case: ternary operator assignment to function pointer */
void handler_a(void) {}
void handler_b(void) {}

int condition = 1;

void (*fp)(void);

int main(void) {
    if (condition) {
        fp = handler_a;
    } else {
        fp = handler_b;
    }
    fp();
    return 0;
}
