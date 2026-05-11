/* Test fixture: field_call complex — global struct with designated initializer */

struct context {
    void (*read)(void *c, char *buf, int len);
    void (*write)(void *c, const char *buf, int len);
};

void net_read(void *c, char *buf, int len) {}
void net_write(void *c, const char *buf, int len) {}

struct context ctx_data = {
    .read = net_read,
    .write = net_write,
};

int main(void) {
    ctx_data.read(&ctx_data, NULL, 0);
    ctx_data.write(&ctx_data, NULL, 0);
    return 0;
}
