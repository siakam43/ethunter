/* Test fixture: initializer_assign complex — multiple structs and array init */

struct handler {
    void (*on_start)(void);
    void (*on_stop)(void);
};

void start_a(void) {}
void stop_a(void) {}
void start_b(void) {}
void stop_b(void) {}

struct handler handlers[] = {
    { start_a, stop_a },
    { start_b, stop_b },
};

struct ops {
    int (*create)(void);
    int (*destroy)(void);
};

int create_item(void) { return 0; }
int destroy_item(void) { return 0; }

struct ops item_ops = {
    .create = create_item,
    .destroy = destroy_item,
};

int main(void) {
    handlers[0].on_start();
    item_ops.create();
    return 0;
}
