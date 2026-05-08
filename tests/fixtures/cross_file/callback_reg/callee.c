/* callback_reg cross-file: callee.c defines registration function */
typedef void (*event_cb)(int);

void on_start(int x) {}
void on_stop(int x) {}

void register_callback(event_cb cb) {}

void setup(void) {
    register_callback(on_start);
    register_callback(on_stop);
}
