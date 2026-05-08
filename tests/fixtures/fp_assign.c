/* Test fixture 2: Function pointer assignment + call */

void foo(void) {}
void bar(void) {}

int main(void) {
    void (*fp)(void) = foo;
    fp();
    fp = bar;
    fp();
    return 0;
}
