/* Complex vtable: multiple struct vtables with partial initialization */

struct device {
    int (*open)(void);
    int (*close)(void);
    int (*read)(char *buf, int len);
};

int net_open(void) { return 0; }
int net_close(void) { return 0; }
int net_read(char *buf, int len) { return 0; }

int disk_open(void) { return 0; }
int disk_close(void) { return 0; }

int main(void) {
    struct device net;
    net.open = net_open;
    net.close = net_close;
    net.read = net_read;
    net.open();
    net.close();

    struct device disk;
    disk.open = disk_open;
    disk.close = disk_close;
    disk.open();
    return 0;
}
