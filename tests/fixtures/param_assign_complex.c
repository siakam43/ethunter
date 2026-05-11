/* Test fixture: param_assign complex — multiple callback parameters */

typedef void (*event_cb)(int);

void register_callback(event_cb cb) {}

void on_start(int code) {}
void on_stop(int code) {}

typedef void (*process_fn)(void *);

void execute(process_fn fn, void *data) {
    fn(data);
}

void worker(void *d) {}

int main(void) {
    register_callback(on_start);
    register_callback(on_stop);
    execute(worker, NULL);
    return 0;
}
