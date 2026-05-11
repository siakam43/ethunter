/* Test fixture: field_call_subscript — struct array subscript field calls */
/* Tests: arr[i]->field() pattern */

typedef struct handler handler_t;
struct handler {
    void (*process)(void);
    void (*cleanup)(void);
};

void handler_a(void) {}
void handler_b(void) {}
void cleanup_a(void) {}
void cleanup_b(void) {}

handler_t handlers[] = {
    { handler_a, cleanup_a },
    { handler_b, cleanup_b },
};

void dispatch(int idx) {
    handlers[idx]->process();
    handlers[idx]->cleanup();
}
