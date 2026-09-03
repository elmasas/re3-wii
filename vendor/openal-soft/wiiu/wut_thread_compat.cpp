#include "wut_thread_compat.h"

extern int __wut_key_create(__wut_key_t *key, void (*dtor) (void *));
extern int __wut_key_delete(__wut_key_t key);
extern void *__wut_getspecific(__wut_key_t key);
extern int __wut_setspecific(__wut_key_t key, const void *ptr);

extern "C" {

int wut_key_create(__wut_key_t *key, void (*dtor) (void *))
{
    return __wut_key_create(key, dtor);
}

int wut_key_delete(__wut_key_t key)
{
    return __wut_key_delete(key);
}

void* wut_getspecific(__wut_key_t key)
{
    return __wut_getspecific(key);
}

int wut_setspecific(__wut_key_t key, const void *ptr)
{
    return __wut_setspecific(key, ptr);
}

}