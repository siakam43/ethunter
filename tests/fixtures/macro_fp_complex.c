/* Complex macro_fp: multiple macros with function references */

#define CALL_ONE(fn) fn()
#define CALL_BOTH(a, b) a(); b()
#define DISPATCH(fn, val) fn(val)

void handler_x(void) {}
void handler_y(void) {}
void handler_z(int v) {}

int main(void) {
    CALL_ONE(handler_x);
    CALL_BOTH(handler_x, handler_y);
    return 0;
}
