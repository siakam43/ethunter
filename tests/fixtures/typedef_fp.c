/* Test fixture 9: Typedef-hidden function pointer */
typedef void (*action_fn)(int);

void do_action(int x) {}
void undo_action(int x) {}

int main(void) {
    action_fn fn = do_action;
    fn(1);
    fn = undo_action;
    fn(2);
    return 0;
}
