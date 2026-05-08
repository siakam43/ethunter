/* Complex callback_reg: multiple registration sites */

typedef void (*hook_t)(int);

void register_hook(hook_t h) {}
void register_exit_hook(hook_t h) {}

void on_connect(int fd) {}
void on_disconnect(int fd) {}
void on_error(int fd) {}
void cleanup(int fd) {}

int main(void) {
    register_hook(on_connect);
    register_hook(on_disconnect);
    register_hook(on_error);
    register_exit_hook(cleanup);
    return 0;
}
