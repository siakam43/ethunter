/* Test fixture: cast_assign — cast expression function pointer assignment */
/* Tests: fn_t *fp = (fn_t *)func_name */

typedef void (update_fn)(void *);

void update_impl(void *r) {}

update_fn *const fp_update = (update_fn *)update_impl;

int main(void) {
    int data = 42;
    fp_update(&data);
    return 0;
}
