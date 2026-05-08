/* Complex fp_return: multiple return-value functions with switch */

typedef void (*action_t)(void);

void action_read(void) {}
void action_write(void) {}
void action_delete(void) {}

action_t get_action(const char *op) {
    if (op[0] == 'r') return action_read;
    if (op[0] == 'w') return action_write;
    return action_delete;
}

int main(void) {
    get_action("read")();
    get_action("write")();
    return 0;
}
