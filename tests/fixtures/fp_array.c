/* Test fixture 5: Function pointer array / dispatch table */
void cmd_help(void) {}
void cmd_quit(void) {}
void cmd_list(void) {}

void (*dispatch[])(void) = { cmd_help, cmd_quit, cmd_list };

int main(void) {
    dispatch[0]();
    dispatch[1]();
    return 0;
}
