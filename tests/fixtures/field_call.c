/* Test fixture: field_call — struct field expression calls */
/* Tests: obj.field() and ptr->field() */

struct driver {
    int (*init)(void);
    int (*read)(char *buf);
};

int fs_init(void) { return 0; }
int fs_read(char *buf) { return 0; }

int main(void) {
    struct driver d;
    d.init = fs_init;
    d.read = fs_read;
    d.init();
    d.read(NULL);
    return 0;
}
