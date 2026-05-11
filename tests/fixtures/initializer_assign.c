/* Test fixture: initializer_assign — designated initializer pattern */
/* Tests init_declarator + initializer_list + pair_list: struct s = { .field = func } */

struct ops {
    int (*init)(void);
    int (*read)(char *buf);
    void (*write)(const char *buf);
};

int fs_init(void) { return 0; }
int fs_read(char *buf) { return 0; }
void fs_write(const char *buf) {}

struct ops file_ops = {
    .init = fs_init,
    .read = fs_read,
    .write = fs_write,
};

int main(void) {
    file_ops.init();
    return 0;
}
