/* Test fixture 7: Callback registration */
typedef void (*event_cb)(int);

void register_callback(event_cb cb) {}

void on_start(int code) {}
void on_stop(int code) {}

int main(void) {
    register_callback(on_start);
    register_callback(on_stop);
    return 0;
}
