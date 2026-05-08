/* union_fp cross-file: callee.c defines union and initializes */
typedef void (*act_t)(void);

union action {
    act_t fn;
};

void do_work(void) {}
void do_cleanup(void) {}

union action a;

void setup(void) {
    a.fn = do_work;
}
