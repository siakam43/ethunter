/* Edge case: macro with function name substring collision */
void close_file(void) {}
void open_session(void) {}

/* This macro contains "close" and "open" as substrings but doesn't call them */
#define HANDLE_CLOSE(x) ((x) + 1)
#define MY_OPEN_FLAG 1

int main(void) {
    int r = HANDLE_CLOSE(5);
    close_file();
    open_session();
    return r;
}
