/* Test fixture 6: Struct vtable-style calls */
struct driver {
    int (*init)(void);
    int (*read)(char *buf);
    int (*write)(const char *buf);
};

int fs_init(void) { return 0; }
int fs_read(char *buf) { return 0; }
int fs_write(const char *buf) { return 0; }

int main(void) {
    struct driver d;
    d.init = fs_init;
    d.read = fs_read;
    d.write = fs_write;
    d.init();
    return 0;
}
