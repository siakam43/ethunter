/* Test fixture 8: Union function pointer */
typedef void (*simple_action)(void);
typedef void (*param_action)(int);

union action {
    simple_action sa;
    param_action pa;
};

void act_simple(void) {}
void act_param(int x) {}

int main(void) {
    union action a;
    a.sa = act_simple;
    a.sa();
    return 0;
}
