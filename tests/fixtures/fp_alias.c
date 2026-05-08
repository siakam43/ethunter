/* Test fixture 10: Function pointer alias chain */
void target_a(void) {}
void target_b(void) {}

int main(void) {
    void (*fp1)(void) = target_a;
    void (*fp2)(void) = fp1;
    fp2();
    return 0;
}
