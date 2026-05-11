/* Test fixture: cast_assign complex — multiple cast patterns */

typedef int (*md5_init_func)(void *);
typedef int (*md5_update_func)(void *, const unsigned char *, unsigned int);

struct md5_params {
    md5_init_func init_func;
    md5_update_func update_func;
};

int my_md5_init(void *ctx) { return 0; }
int my_md5_update(void *ctx, const unsigned char *data, unsigned int len) { return 0; }

/* Cast in init_declarator */
md5_init_func g_init = (md5_init_func)my_md5_init;

/* Cast in assignment */
int main(void) {
    struct md5_params p;
    p.init_func = (md5_init_func)my_md5_init;
    p.update_func = (md5_update_func)my_md5_update;
    p.init_func(NULL);
    return 0;
}
