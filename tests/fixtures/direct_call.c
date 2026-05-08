/* Test fixture 1: Direct function calls */

void helper(void) {
}

void worker(int x) {
    helper();
}

void main(void) {
    worker(42);
    helper();
}
