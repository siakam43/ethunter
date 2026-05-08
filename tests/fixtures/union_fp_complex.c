/* Complex union_fp: multiple union types with member access */

typedef void (*action_simple)(void);
typedef void (*action_param)(int);

union operation {
    action_simple simple;
    action_param with_param;
};

void op_init(void) {}
void op_process(int x) {}
void op_finish(void) {}

int main(void) {
    union operation op;
    op.simple = op_init;
    op.simple();
    op.with_param = op_process;
    op.with_param(42);
    op.simple = op_finish;
    op.simple();
    return 0;
}
