/* vtable cross-file: callee.c defines struct and assigns members */
struct driver {
    int (*init)(void);
    int (*read)(void);
};

int dev_init(void) { return 0; }
int dev_read(void) { return 0; }

struct driver d;

void setup(void) {
    d.init = dev_init;
    d.read = dev_read;
}
