/* fp_return cross-file: callee.c defines the function returning fp */
typedef void (*action_t)(void);

void action_a(void) {}
void action_b(void) {}

action_t get_handler(int type) {
    if (type == 0) return action_a;
    return action_b;
}
