/* union_fp cross-file: caller.c calls through union member */
typedef void (*act_t)(void);

union action {
    act_t fn;
};

extern union action a;
void setup(void);

void caller_func(void) {
    setup();
    a.fn();
}
