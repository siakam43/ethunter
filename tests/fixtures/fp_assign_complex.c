/* Complex fp_assign: conditional reassignment + alias chain */

void handler_a(void) {}
void handler_b(void) {}
void handler_c(void) {}

void dispatch(int mode) {
    void (*fp)(void) = handler_a;
    if (mode > 0) {
        fp = handler_b;
    } else {
        fp = handler_c;
    }
    fp();
    void (*fp2)(void) = fp;
    fp2();
}

int main(void) {
    dispatch(1);
    return 0;
}
