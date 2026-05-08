/* fp_array cross-file: callee.c defines the dispatch table */
void cmd_read(void) {}
void cmd_write(void) {}

void (*table[])(void) = { cmd_read, cmd_write };
