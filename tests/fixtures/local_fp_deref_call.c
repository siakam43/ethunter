/* Test fixture: pointer dereference call through local variable */

struct handler {
    void (*process)(void);
};

static void default_process(void) {
    return;
}

/* Global struct initializer — tracked by initializer_assign as <gstruct:global_handler.process> */
static struct handler global_handler = {
    .process = default_process,
};

void caller(void) {
    /* init_declarator with field_expression RHS */
    void (*local)(void) = global_handler.process;
    (*local)();
}
