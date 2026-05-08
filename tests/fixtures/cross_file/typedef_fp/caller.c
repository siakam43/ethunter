/* typedef_fp cross-file: caller.c uses typedef from callee.h */
typedef void (*process_fn)(int);

void process_a(int x);
void process_b(int x);

void caller_func(void) {
    process_fn fn = process_a;
    fn(1);
    fn = process_b;
    fn(2);
}
