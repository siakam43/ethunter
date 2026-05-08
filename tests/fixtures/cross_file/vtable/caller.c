/* vtable cross-file: caller.c calls through struct members */
struct driver {
    int (*init)(void);
    int (*read)(void);
};

extern struct driver d;
void setup(void);

void caller_func(void) {
    setup();
    d.init();
    d.read();
}
