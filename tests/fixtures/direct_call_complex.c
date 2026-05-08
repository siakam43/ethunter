/* Complex direct call scenario: nested chains, loops, conditionals */

void leaf_a(void) {}
void leaf_b(void) {}
void leaf_c(void) {}

void middle_one(void) {
    leaf_a();
    leaf_b();
}

void middle_two(int x) {
    if (x > 0) {
        leaf_c();
    }
    middle_one();
}

void top(void) {
    middle_two(1);
    middle_two(0);
    leaf_a();

    for (int i = 0; i < 3; i++) {
        leaf_b();
    }
}
