/* Edge case: long alias chain (3+ links) */
void target_func(void) {}

int main(void) {
    void (*fp1)(void) = target_func;
    void (*fp2)(void) = fp1;
    void (*fp3)(void) = fp2;
    void (*fp4)(void) = fp3;
    fp4();
    return 0;
}
