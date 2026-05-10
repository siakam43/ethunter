/* ET-Bench fixture: fnptr-varargs/example_1 */
/* fnptr: share->lockfunc, targets: my_lock */
/* Pattern: va_list with function pointer stored in struct, called through varargs function */

#include <stdarg.h>
#include <stddef.h>

/* Void pointer type for va_arg compatibility with tree-sitter */
typedef void *void_ptr_t;

typedef enum {
    CURLSHE_OK = 0,
    CURLSHE_INVALID,
    CURLSHE_IN_USE,
    CURLSHE_BAD_OPTION
} CURLSHcode;

typedef enum {
    CURLSHOPT_NONE = 0,
    CURLSHOPT_SHARE,
    CURLSHOPT_LOCKFUNC,
    CURLSHOPT_UNLOCKFUNC,
    CURLSHOPT_USERDATA
} CURLSHoption;

typedef enum {
    CURL_LOCK_DATA_CONNECT = 0,
    CURL_LOCK_DATA_DNS,
    CURL_LOCK_DATA_SSL_SESSION,
    CURL_LOCK_DATA_COOKIE,
    CURL_LOCK_DATA_SHARE
} curl_lock_data;

typedef enum {
    CURL_LOCK_ACCESS_NONE = 0,
    CURL_LOCK_ACCESS_SINGLE,
    CURL_LOCK_ACCESS_SHARED
} curl_lock_access;

typedef void (*curl_lock_function)(void *userptr,
                                   curl_lock_data data,
                                   curl_lock_access locktype,
                                   void *userptr_data);
typedef void (*curl_unlock_function)(void *userptr,
                                     curl_lock_data data,
                                     void *userptr_data);

struct Curl_share {
    int dirty;
    curl_lock_function lockfunc;
    curl_unlock_function unlockfunc;
    void *clientdata;
    int options[16];
};

typedef struct Curl_share CURLSH;

#define GOOD_SHARE_HANDLE(s) ((s) != NULL)

/* va_list-based varargs function that stores a function pointer in struct */
CURLSHcode curl_share_setopt(struct Curl_share *share, CURLSHoption option, ...)
{
    va_list param;
    int type;
    curl_lock_function lockfunc;
    curl_unlock_function unlockfunc;
    void *ptr;
    CURLSHcode res = CURLSHE_OK;

    if (!GOOD_SHARE_HANDLE(share))
        return CURLSHE_INVALID;

    if (share->dirty)
        return CURLSHE_IN_USE;

    va_start(param, option);

    switch (option) {
    case CURLSHOPT_SHARE:
        type = va_arg(param, int);
        switch (type) {
        case CURL_LOCK_DATA_DNS:
            share->options[CURL_LOCK_DATA_DNS] = 1;
            break;
        case CURL_LOCK_DATA_SSL_SESSION:
            share->options[CURL_LOCK_DATA_SSL_SESSION] = 1;
            break;
        case CURL_LOCK_DATA_CONNECT:
            share->options[CURL_LOCK_DATA_CONNECT] = 1;
            break;
        default:
            res = CURLSHE_BAD_OPTION;
            break;
        }
        break;

    case CURLSHOPT_LOCKFUNC:
        lockfunc = va_arg(param, curl_lock_function);
        share->lockfunc = lockfunc;
        break;

    case CURLSHOPT_UNLOCKFUNC:
        unlockfunc = va_arg(param, curl_unlock_function);
        share->unlockfunc = unlockfunc;
        break;

    case CURLSHOPT_USERDATA:
        ptr = (void *)va_arg(param, void_ptr_t);
        share->clientdata = ptr;
        break;

    default:
        res = CURLSHE_BAD_OPTION;
        break;
    }

    va_end(param);
    return res;
}

/* Target function: called through share->lockfunc in curl_share_cleanup */
void my_lock(void *userptr, curl_lock_data data,
             curl_lock_access locktype, void *userptr_data)
{
    (void)userptr;
    (void)data;
    (void)locktype;
    (void)userptr_data;
}

void my_unlock(void *userptr, curl_lock_data data, void *userptr_data)
{
    (void)userptr;
    (void)data;
    (void)userptr_data;
}

/* Setup helper: configures share object with lock functions */
static void share_setup(struct Curl_share *share)
{
    share->dirty = 0;
    share->lockfunc = my_lock;
    share->unlockfunc = my_unlock;
    share->clientdata = NULL;

    curl_share_setopt(share, CURLSHOPT_SHARE, CURL_LOCK_DATA_CONNECT);
    curl_share_setopt(share, CURLSHOPT_LOCKFUNC, my_lock);
    curl_share_setopt(share, CURLSHOPT_UNLOCKFUNC, my_unlock);
}

CURLSHcode
curl_share_cleanup(struct Curl_share *share)
{
    if (!GOOD_SHARE_HANDLE(share))
        return CURLSHE_INVALID;

    if (share->lockfunc)
        share->lockfunc(NULL, CURL_LOCK_DATA_SHARE, CURL_LOCK_ACCESS_SINGLE,
                        share->clientdata);

    share->dirty = 0;
    share->lockfunc = NULL;
    share->unlockfunc = NULL;

    return CURLSHE_OK;
}

int main(void)
{
    struct Curl_share share_obj;
    struct Curl_share *share = &share_obj;

    share_setup(share);
    curl_share_cleanup(share);
    return 0;
}
