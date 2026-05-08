/* Complex typedef_fp: multiple typedef layers */

typedef void (*base_handler)(void);
typedef base_handler handler_wrapper;

void handle_request(void) {}
void handle_response(void) {}

int main(void) {
    handler_wrapper hw = handle_request;
    hw();
    hw = handle_response;
    hw();
    return 0;
}
