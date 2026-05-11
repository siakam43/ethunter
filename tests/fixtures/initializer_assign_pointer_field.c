/* Test fixture: initializer_assign_pointer_field — runtime struct pointer field assignment */
/* Tests: vec->field = func where vec = &global_array[i], with literal function names */

typedef struct ops ops_t;
struct ops {
    void (*process)(void);
    void (*cleanup)(void);
};

void handler_a(void) {}
void handler_b(void) {}
void cleanup_a(void) {}
void cleanup_b(void) {}

static ops_t dispatch_table[2];

void init(void)
{
    ops_t *vec = &dispatch_table[0];
    vec->process = handler_a;
    vec->cleanup = cleanup_a;

    vec = &dispatch_table[1];
    vec->process = handler_b;
    vec->cleanup = cleanup_b;
}

void dispatch(int idx)
{
    dispatch_table[idx]->process();
    dispatch_table[idx]->cleanup();
}
