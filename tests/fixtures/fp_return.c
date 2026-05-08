/* Test fixture 4: Function pointer return value */
typedef void (*handler_t)(void);

void action_a(void) {}
void action_b(void) {}

handler_t get_handler(int type) {
    if (type == 1) return action_a;
    return action_b;
}

int main(void) {
    get_handler(1)();
    return 0;
}
