/* Complex lazy_init: multiple lazy-initialized pointers */

static void (*primary_handler)(void) = (void *)0;
static void (*secondary_handler)(void) = (void *)0;

void default_primary(void) {}
void custom_primary(void) {}
void default_secondary(void) {}

void init_primary(int use_custom) {
    if (!primary_handler) {
        if (use_custom) {
            primary_handler = custom_primary;
        } else {
            primary_handler = default_primary;
        }
    }
}

void init_secondary(void) {
    if (!secondary_handler) {
        secondary_handler = default_secondary;
    }
}

int main(void) {
    init_primary(1);
    primary_handler();
    init_secondary();
    secondary_handler();
    return 0;
}
