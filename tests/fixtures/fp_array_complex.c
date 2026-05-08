/* Complex fp_array: multiple dispatch tables with constant indices */

void cmd_create(void) {}
void cmd_read(void) {}
void cmd_update(void) {}
void cmd_delete(void) {}

void (*cmd_table[])(void) = { cmd_create, cmd_read, cmd_update, cmd_delete };

enum { CREATE = 0, READ, UPDATE, DELETE };

int process(int cmd) {
    cmd_table[cmd]();
    cmd_table[READ]();
    return 0;
}

int main(void) {
    process(CREATE);
    return 0;
}
