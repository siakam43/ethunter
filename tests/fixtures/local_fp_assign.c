/* Test fixture: local variable inherits function pointer from struct field */

struct ops {
    int (*compute)(int x);
};

static int double_it(int x) {
    return x * 2;
}

/* Global struct initializer — tracked by initializer_assign as <gstruct:global_ops.compute> */
static struct ops global_ops = {
    .compute = double_it,
};

void caller(void) {
    /* init_declarator with field_expression RHS */
    int (*fn)(int) = global_ops.compute;
    fn(42);
}

void caller_assign(void) {
    /* assignment_expression with field_expression RHS */
    int (*fn2)(int);
    fn2 = global_ops.compute;
    fn2(42);
}
