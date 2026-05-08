/* Complex fp_alias: multi-level alias chains (3+ levels) using assignment expressions */

void target_one(void) {}
void target_two(void) {}

int main(void) {
    void (*fp1)(void);
    void (*fp2)(void);
    void (*fp3)(void);
    void (*fp4)(void);
    void (*fp5)(void);

    fp1 = target_one;
    fp2 = fp1;
    fp3 = fp2;
    fp4 = target_two;
    fp5 = fp3;  /* fp5 -> fp3 -> fp2 -> fp1 -> target_one */
    fp5();
    fp4();
    return 0;
}
