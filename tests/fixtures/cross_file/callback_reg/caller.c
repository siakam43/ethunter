/* callback_reg cross-file: caller.c registers a local callback */
typedef void (*event_cb)(int);

void register_callback(event_cb cb);
void setup(void);

void local_event(int x) {}

void main_func(void) {
    register_callback(local_event);
    setup();
}
