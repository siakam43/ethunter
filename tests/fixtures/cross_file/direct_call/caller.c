/* direct_call cross-file: caller.c */
void helper(void);
void worker(int x);

void main_func(void) {
    helper();
    worker(42);
}
